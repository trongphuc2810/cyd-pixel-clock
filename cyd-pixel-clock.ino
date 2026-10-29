#include <Arduino.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <HTTPUpdate.h>
#include <ArduinoJson.h>
#include <TFT_eSPI.h>
#include <SPI.h>
#include <XPT2046_Touchscreen.h>
#include <Preferences.h>
#include <time.h>

#include "config.h"
#include "muse_pixel.h"

// ── Hardware Instances ──────────────────────────────────────────────────────
TFT_eSPI tft = TFT_eSPI();
SPIClass touchSPI(HSPI);
XPT2046_Touchscreen ts(TOUCH_CS, TOUCH_IRQ);
Preferences prefs;

// ── Application State ───────────────────────────────────────────────────────
muse_mode_t currentMode = MUSE_MODE_IDLE;
muse_mode_t baseMode = MUSE_MODE_IDLE; // User selected: IDLE or LAPTOP (Phone)
bool isHappyReaction = false;
bool cfgInvert = true; // FIX: CYD ST7789 panel requires inverted display colors
unsigned long reactionStartTime = 0;
unsigned long modeStartTime = 0;
unsigned long lastTouchTime = 0;
unsigned long lastFrameTime = 0;
unsigned long lastClockUpdate = 0;
unsigned long lastOTACheck = 0;

int lastSec = -1;
int lastMin = -1;
int lastHour = -1;
int lastMday = -1;

#define AVATAR_SIZE 120
#define AVATAR_X 5
#define AVATAR_Y 40

// Days of week in English abbreviation
const char *DAYS_OF_WEEK[] = { "SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT" };

// ── Forward Declarations ────────────────────────────────────────────────────
void switchMode(muse_mode_t mode);
void drawStaticUI();
void updateClockDisplay();
void drawActionButtons();
void drawWiFiStatus(bool connected);
void drawOTAButton();
void checkGitHubOTA(bool manual);
void renderMuseFrame();
void showAPSetupScreen();

// ── WiFiManager AP Callback ─────────────────────────────────────────────────
void configModeCallback(WiFiManager *myWiFiManager) {
    showAPSetupScreen();
}

// ============================================================================
// Setup
// ============================================================================
void setup() {
    Serial.begin(115200);
    Serial.println("\n[BOOT] CYD Pixel Clock starting...");

    pinMode(BOOT_BTN, INPUT_PULLUP);

    // 1. Initialize Display
    tft.init();
    tft.setRotation(1); // 320x240 landscape

    // Load inversion preference (Default TRUE for CYD ST7789)
    prefs.begin("cyd_clock", false);
    cfgInvert = prefs.getBool("invert", true);
    tft.invertDisplay(cfgInvert);

    tft.fillScreen(COLOR_BG);

    // Backlight on CYD
    pinMode(TFT_BL, OUTPUT);
    digitalWrite(TFT_BL, HIGH);

    // 2. Initialize Touchscreen
    touchSPI.begin(TOUCH_CLK, TOUCH_MISO, TOUCH_MOSI, TOUCH_CS);
    ts.begin(touchSPI);
    ts.setRotation(1);

    // 3. Initialize Muse Engine
    muse_pixel_set_size(AVATAR_SIZE);
    switchMode(MUSE_MODE_IDLE);

    // 4. WiFi Setup via WiFiManager (Zero hardcoded secrets)
    WiFiManager wm;
    wm.setAPCallback(configModeCallback);
    wm.setConfigPortalTimeout(180); // 3 minutes timeout if unattended

    bool forcePortal = (digitalRead(BOOT_BTN) == LOW);
    bool wifiConnected = false;

    if (forcePortal) {
        Serial.println("[WIFI] BOOT button pressed - forcing AP mode");
        showAPSetupScreen();
        wifiConnected = wm.startConfigPortal(AP_SSID, AP_PASS);
    } else {
        // Show connecting splash
        tft.fillScreen(COLOR_BG);
        tft.setTextColor(COLOR_WHITE, COLOR_BG);
        tft.setTextDatum(MC_DATUM);
        tft.drawString("Connecting WiFi...", 160, 110, 4);
        tft.drawString(AP_SSID, 160, 145, 2);

        wifiConnected = wm.autoConnect(AP_SSID, AP_PASS);
    }

    // 5. Clear Screen & Draw Initial Layout
    tft.fillScreen(COLOR_BG);
    drawStaticUI();
    drawWiFiStatus(wifiConnected);

    // 6. Time Synchronization via NTP
    if (wifiConnected) {
        configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET, NTP_SERVER_1, NTP_SERVER_2);
        Serial.println("[NTP] Time configured via NTP");

        // Initial check for GitHub OTA update (silent)
        checkGitHubOTA(false);
        lastOTACheck = millis();
    }

    lastTouchTime = millis();
    lastClockUpdate = millis();
}

// ============================================================================
// Main Loop
// ============================================================================
void loop() {
    unsigned long now = millis();

    // 1. Touch Handling
    if (ts.touched()) {
        TS_Point p = ts.getPoint();
        int tx = map(p.x, TS_MINX, TS_MAXX, 0, 320);
        int ty = map(p.y, TS_MINY, TS_MAXY, 0, 240);
        tx = constrain(tx, 0, 319);
        ty = constrain(ty, 0, 239);

        lastTouchTime = now;

        // Special: Tap MUSE logo at top-left (tx < 60 && ty < 24) to toggle display color inversion
        if (tx < 60 && ty < 24) {
            cfgInvert = !cfgInvert;
            tft.invertDisplay(cfgInvert);
            prefs.putBool("invert", cfgInvert);
            Serial.printf("[DISPLAY] Inversion toggled: %d\n", cfgInvert);
            delay(300);
            return;
        }

        // Special: Tap [OTA UPDATE] button at bottom-right (tx >= 220 && ty >= 204)
        if (tx >= 220 && ty >= 204) {
            drawOTAButton();
            checkGitHubOTA(true); // Manual check with visual feedback
            drawStaticUI();
            drawWiFiStatus(WiFi.status() == WL_CONNECTED);
            lastSec = -1; lastMin = -1; lastHour = -1; lastMday = -1;
            return;
        }

        // If Muse was sleeping or bored, wake up immediately
        if (currentMode == MUSE_MODE_SLEEPING || currentMode == MUSE_MODE_BORED) {
            switchMode(baseMode);
        } else {
            // Check tap on left pane (companion area)
            if (tx < 132) {
                if (ty >= 204) {
                    // Action button area
                    if (tx < 66) {
                        // [♥ PET] Button
                        baseMode = MUSE_MODE_IDLE;
                        isHappyReaction = false;
                        switchMode(MUSE_MODE_IDLE);
                        drawActionButtons();
                    } else {
                        // [📱 PHONE] Button
                        baseMode = MUSE_MODE_LAPTOP;
                        isHappyReaction = false;
                        switchMode(MUSE_MODE_LAPTOP);
                        drawActionButtons();
                    }
                } else if (ty >= AVATAR_Y && ty < AVATAR_Y + AVATAR_SIZE) {
                    // Tapped the avatar directly: trigger HAPPY reaction or LISTEN
                    isHappyReaction = true;
                    reactionStartTime = now;
                    switchMode(MUSE_MODE_LISTENING);
                }
            }
        }
        delay(120); // Debounce
    }

    // 2. Idle State Logic
    unsigned long idleSec = (now - lastTouchTime) / 1000;
    if (baseMode != MUSE_MODE_LAPTOP) { // If not explicitly set to PHONE mode
        if (isHappyReaction && (now - reactionStartTime) > 3500) {
            // Revert from reaction back to base mode after 3.5s
            isHappyReaction = false;
            switchMode(baseMode);
        } else if (!isHappyReaction) {
            if (idleSec >= 180 && currentMode != MUSE_MODE_SLEEPING) {
                switchMode(MUSE_MODE_SLEEPING);
            } else if (idleSec >= 60 && idleSec < 180 && currentMode != MUSE_MODE_BORED && currentMode != MUSE_MODE_SLEEPING) {
                switchMode(MUSE_MODE_BORED);
            }
        }
    }

    // 3. Render Avatar Frame at 25 FPS (every 40ms)
    if (now - lastFrameTime >= 40) {
        lastFrameTime = now;
        renderMuseFrame();
    }

    // 4. Update Clock & Date (every 1s)
    if (now - lastClockUpdate >= 500) {
        lastClockUpdate = now;
        updateClockDisplay();
    }

    // 5. Periodic GitHub OTA Check (every 24 hours, silent)
    if (WiFi.status() == WL_CONNECTED && (now - lastOTACheck >= 86400000UL)) {
        lastOTACheck = now;
        checkGitHubOTA(false);
    }
}

// ============================================================================
// UI Rendering Functions
// ============================================================================

void switchMode(muse_mode_t mode) {
    currentMode = mode;
    modeStartTime = millis();

    // Update status badge in left pane header
    tft.fillRect(60, 6, 68, 14, COLOR_BG);
    tft.setTextColor(COLOR_GRAY_MID, COLOR_BG);
    tft.setTextDatum(TR_DATUM);

    const char *stateStr = "IDLE";
    if (isHappyReaction || mode == MUSE_MODE_LISTENING) stateStr = "LISTEN";
    else if (mode == MUSE_MODE_LAPTOP) stateStr = "PHONE";
    else if (mode == MUSE_MODE_BORED) stateStr = "BORED";
    else if (mode == MUSE_MODE_SLEEPING) stateStr = "SLEEP";

    tft.drawString(stateStr, 126, 6, 1);
}

void drawStaticUI() {
    // 1. Vertical dashed divider
    for (int y = 0; y < 240; y += 4) {
        tft.drawPixel(131, y, COLOR_DIVIDER);
        tft.drawPixel(131, y + 1, COLOR_DIVIDER);
    }

    // 2. Left Header
    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(COLOR_WHITE, COLOR_BG);
    tft.drawString("MUSE", 8, 6, 2);

    // 3. Left Action buttons
    drawActionButtons();

    // 4. Right Top Status divider
    tft.drawFastHLine(140, 22, 172, COLOR_DIVIDER);

    // 5. Right Bottom divider & hints
    tft.drawFastHLine(140, 214, 172, COLOR_DIVIDER);
    tft.setTextDatum(BL_DATUM);
    tft.setTextColor(COLOR_GRAY_MID, COLOR_BG);
    tft.drawString("TAP TO REACT", 140, 234, 1);

    // 6. Right Bottom [OTA UPDATE] Button
    drawOTAButton();
}

void drawActionButtons() {
    // [♥ PET] Button at (6, 208, 58, 22)
    bool petActive = (baseMode == MUSE_MODE_IDLE);
    tft.fillRoundRect(6, 208, 58, 22, 3, petActive ? COLOR_WHITE : COLOR_GRAY_DARK);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(petActive ? COLOR_BG : COLOR_GRAY_LIGHT, petActive ? COLOR_WHITE : COLOR_GRAY_DARK);
    tft.drawString("PET", 35, 219, 1);

    // [📱 PHONE] Button at (68, 208, 58, 22)
    bool phoneActive = (baseMode == MUSE_MODE_LAPTOP);
    tft.fillRoundRect(68, 208, 58, 22, 3, phoneActive ? COLOR_WHITE : COLOR_GRAY_DARK);
    tft.setTextColor(phoneActive ? COLOR_BG : COLOR_GRAY_LIGHT, phoneActive ? COLOR_WHITE : COLOR_GRAY_DARK);
    tft.drawString("PHONE", 97, 219, 1);
}

void drawOTAButton() {
    tft.fillRoundRect(236, 217, 76, 20, 2, COLOR_GRAY_DARK);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(COLOR_WHITE, COLOR_GRAY_DARK);
    tft.drawString("OTA UPDATE", 274, 227, 1);
}

void drawWiFiStatus(bool connected) {
    tft.fillRect(140, 4, 172, 16, COLOR_BG);

    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(connected ? COLOR_WHITE : COLOR_GRAY_MID, COLOR_BG);

    if (connected) {
        String ssid = WiFi.SSID();
        if (ssid.length() > 14) ssid = ssid.substring(0, 11) + "...";
        tft.drawString(ssid, 155, 6, 1);

        // Signal bars
        tft.fillRect(142, 15, 2, 3, COLOR_WHITE);
        tft.fillRect(145, 13, 2, 5, COLOR_WHITE);
        tft.fillRect(148, 10, 2, 8, COLOR_WHITE);

        tft.setTextDatum(TR_DATUM);
        tft.setTextColor(COLOR_GRAY_MID, COLOR_BG);
        tft.drawString("CONNECTED", 312, 6, 1);
    } else {
        tft.drawString("OFFLINE", 142, 6, 1);
        tft.setTextDatum(TR_DATUM);
        tft.setTextColor(COLOR_GRAY_DARK, COLOR_BG);
        tft.drawString("NO WIFI", 312, 6, 1);
    }
}

void updateClockDisplay() {
    time_t nowTime;
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
        return;
    }

    // Check if time changed
    if (timeinfo.tm_sec == lastSec && timeinfo.tm_min == lastMin) {
        return;
    }

    // 1. Update Hours & Minutes (Font 6: 48px)
    if (timeinfo.tm_min != lastMin || timeinfo.tm_hour != lastHour) {
        lastMin = timeinfo.tm_min;
        lastHour = timeinfo.tm_hour;

        char hmStr[16];
        snprintf(hmStr, sizeof(hmStr), "%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min);

        tft.fillRect(140, 52, 128, 54, COLOR_BG);
        tft.setTextColor(COLOR_WHITE, COLOR_BG);
        tft.setTextDatum(TL_DATUM);
        tft.drawString(hmStr, 142, 54, 6);
    }

    // 2. Update Seconds (Font 4: 26px)
    if (timeinfo.tm_sec != lastSec) {
        lastSec = timeinfo.tm_sec;

        char secStr[8];
        snprintf(secStr, sizeof(secStr), "%02d", timeinfo.tm_sec);

        tft.fillRect(272, 64, 40, 32, COLOR_BG);
        tft.setTextColor(COLOR_GRAY_MID, COLOR_BG);
        tft.setTextDatum(TL_DATUM);
        tft.drawString(secStr, 274, 66, 4);
    }

    // 3. Update Date & Day (Font 2)
    if (timeinfo.tm_mday != lastMday) {
        lastMday = timeinfo.tm_mday;

        tft.fillRect(140, 116, 172, 28, COLOR_BG);

        // Day abbreviation badge (Inverted white box)
        const char *dayStr = DAYS_OF_WEEK[timeinfo.tm_wday];
        tft.fillRoundRect(142, 118, 36, 18, 2, COLOR_WHITE);
        tft.setTextDatum(MC_DATUM);
        tft.setTextColor(COLOR_BG, COLOR_WHITE);
        tft.drawString(dayStr, 160, 127, 2);

        // ISO Date String: YYYY-MM-DD
        char dateStr[16];
        snprintf(dateStr, sizeof(dateStr), "%04d-%02d-%02d",
                 timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday);

        tft.setTextDatum(ML_DATUM);
        tft.setTextColor(COLOR_GRAY_LIGHT, COLOR_BG);
        tft.drawString(dateStr, 186, 127, 2);
    }
}

void renderMuseFrame() {
    muse_pose_t pose = {
        .mode = currentMode,
        .t = millis() / 1000.0f,
        .mode_t = (millis() - modeStartTime) / 1000.0f,
        .level = isHappyReaction ? 0.7f : 0.0f,
        .happy = isHappyReaction ? 1.0f : 0.0f
    };

    muse_pixel_render(&pose);

    // Push frame in 8-line strips directly over SPI DMA
    uint16_t line_buffer[AVATAR_SIZE * 8];
    for (int y = 0; y < AVATAR_SIZE; y += 8) {
        muse_pixel_scale(line_buffer, AVATAR_SIZE, 0, AVATAR_SIZE - 1, y, y + 7);
        tft.pushImage(AVATAR_X, AVATAR_Y + y, AVATAR_SIZE, 8, line_buffer);
    }
}

void showAPSetupScreen() {
    tft.fillScreen(COLOR_BG);

    // Divider
    for (int y = 0; y < 240; y += 4) {
        tft.drawPixel(125, y, COLOR_DIVIDER);
        tft.drawPixel(125, y + 1, COLOR_DIVIDER);
    }

    // Left pane
    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(COLOR_WHITE, COLOR_BG);
    tft.drawString("MUSE", 8, 6, 2);
    tft.fillRoundRect(70, 6, 45, 14, 2, COLOR_GRAY_DARK);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("HOST", 92, 13, 1);

    tft.fillRoundRect(8, 206, 110, 22, 2, COLOR_GRAY_DARK);
    tft.setTextColor(COLOR_WHITE, COLOR_GRAY_DARK);
    tft.drawString("AP ACTIVE", 63, 217, 2);

    // Right pane
    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(COLOR_WHITE, COLOR_BG);
    tft.drawString("WIFI SETUP", 136, 6, 2);
    tft.fillRoundRect(250, 6, 60, 16, 2, COLOR_WHITE);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(COLOR_BG, COLOR_WHITE);
    tft.drawString("AP MODE", 280, 14, 1);

    tft.drawFastHLine(136, 26, 174, COLOR_DIVIDER);

    // AP Card
    tft.fillRoundRect(136, 36, 174, 62, 3, COLOR_GRAY_DARK);
    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(COLOR_GRAY_MID, COLOR_GRAY_DARK);
    tft.drawString("HOTSPOT SSID:", 144, 42, 1);
    tft.setTextColor(COLOR_WHITE, COLOR_GRAY_DARK);
    tft.drawString(AP_SSID, 144, 53, 2);

    tft.setTextColor(COLOR_GRAY_MID, COLOR_GRAY_DARK);
    tft.drawString("CONFIG IP:", 144, 72, 1);
    tft.setTextColor(COLOR_WHITE, COLOR_GRAY_DARK);
    tft.drawString(WiFi.softAPIP().toString(), 144, 83, 2);

    // Instructions
    tft.setTextColor(COLOR_GRAY_LIGHT, COLOR_BG);
    tft.drawString("1. Connect phone to hotspot", 136, 114, 1);
    tft.drawString("2. Open browser or wait pop-up", 136, 132, 1);
    tft.drawString("3. Select WiFi & save password", 136, 150, 1);

    tft.drawFastHLine(136, 214, 174, COLOR_DIVIDER);
    tft.setTextDatum(BL_DATUM);
    tft.setTextColor(COLOR_GRAY_MID, COLOR_BG);
    tft.drawString("WAITING FOR PHONE...", 136, 234, 1);
}

// ============================================================================
// GitHub OTA Updater (Public Releases, Zero Leaks)
// ============================================================================
void checkGitHubOTA(bool manual) {
    Serial.println("[OTA] Checking GitHub for updates...");

    if (manual) {
        tft.fillRect(136, 150, 180, 58, COLOR_BG);
        tft.fillRoundRect(140, 154, 172, 50, 3, COLOR_GRAY_DARK);
        tft.setTextColor(COLOR_WHITE, COLOR_GRAY_DARK);
        tft.setTextDatum(MC_DATUM);
        tft.drawString("CHECKING GITHUB...", 226, 179, 2);
    }

    if (WiFi.status() != WL_CONNECTED) {
        if (manual) {
            tft.fillRoundRect(140, 154, 172, 50, 3, COLOR_GRAY_DARK);
            tft.drawString("NO WIFI CONNECTED", 226, 179, 2);
            delay(2000);
            tft.fillRect(136, 150, 180, 58, COLOR_BG);
        }
        return;
    }

    WiFiClientSecure client;
    client.setInsecure(); // GitHub redirect to AWS S3 CDN

    HTTPClient http;
    String url = String("https://api.github.com/repos/") + GITHUB_REPO + "/releases/latest";
    http.begin(client, url);
    http.addHeader("User-Agent", "CYD-Pixel-Clock");

    int httpCode = http.GET();
    if (httpCode == HTTP_CODE_OK) {
        String payload = http.getString();
        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, payload);
        if (!err) {
            const char* tag_name = doc["tag_name"];
            Serial.printf("[OTA] Latest tag: %s, current: %s\n", tag_name, FIRMWARE_VERSION);

            if (tag_name && strcmp(tag_name, FIRMWARE_VERSION) != 0) {
                // Find .bin download URL
                JsonArray assets = doc["assets"];
                String bin_url = "";
                for (JsonObject asset : assets) {
                    const char* name = asset["name"];
                    if (name && strstr(name, ".bin")) {
                        bin_url = asset["browser_download_url"].as<String>();
                        break;
                    }
                }

                if (bin_url.length() > 0) {
                    Serial.printf("[OTA] Downloading: %s\n", bin_url.c_str());

                    // Visual feedback
                    tft.fillRoundRect(140, 145, 172, 65, 3, COLOR_WHITE);
                    tft.setTextColor(COLOR_BG, COLOR_WHITE);
                    tft.setTextDatum(MC_DATUM);
                    tft.drawString("FOUND NEW RELEASE!", 226, 160, 1);
                    tft.drawString(tag_name, 226, 175, 2);
                    tft.drawString("FLASHING OTA...", 226, 195, 1);

                    switchMode(MUSE_MODE_LAPTOP);

                    // Progress bar callback
                    httpUpdate.onProgress([](int cur, int total) {
                        if (total > 0) {
                            int pct = (cur * 100) / total;
                            tft.fillRect(144, 203, 164, 4, COLOR_GRAY_DARK);
                            tft.fillRect(144, 203, (164 * pct) / 100, 4, COLOR_BG);
                        }
                    });

                    httpUpdate.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
                    t_httpUpdate_return ret = httpUpdate.update(client, bin_url);
                    if (ret == HTTP_UPDATE_OK) {
                        Serial.println("[OTA] Update OK! Rebooting...");
                        tft.drawString("SUCCESS! REBOOT...", 226, 195, 1);
                        delay(1000);
                        ESP.restart();
                    } else {
                        Serial.printf("[OTA] Update failed: %s\n", httpUpdate.getLastErrorString().c_str());
                        tft.drawString("UPDATE FAILED", 226, 195, 1);
                        delay(2000);
                    }
                }
            } else {
                Serial.println("[OTA] Firmware is up to date.");
                if (manual) {
                    tft.fillRoundRect(140, 154, 172, 50, 3, COLOR_GRAY_DARK);
                    tft.setTextColor(COLOR_WHITE, COLOR_GRAY_DARK);
                    tft.drawString("ALREADY LATEST!", 226, 172, 2);
                    tft.drawString(FIRMWARE_VERSION, 226, 190, 1);
                    delay(2000);
                    tft.fillRect(136, 150, 180, 58, COLOR_BG);
                }
            }
        }
    } else {
        if (manual) {
            tft.fillRoundRect(140, 154, 172, 50, 3, COLOR_GRAY_DARK);
            tft.setTextColor(COLOR_WHITE, COLOR_GRAY_DARK);
            tft.drawString("CHECK FAILED", 226, 172, 2);
            tft.drawString(String("HTTP ") + httpCode, 226, 190, 1);
            delay(2000);
            tft.fillRect(136, 150, 180, 58, COLOR_BG);
        }
    }
    http.end();
}
