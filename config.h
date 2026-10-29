#pragma once

// ============================================================================
// CYD Hardware Pin Configuration (ESP32-2432S028R)
// ============================================================================
#define TFT_BL          21
#define BOOT_BTN        0

// Touch Controller (XPT2046 over HSPI)
#define TOUCH_CS        33
#define TOUCH_IRQ       36
#define TOUCH_MOSI      32
#define TOUCH_MISO      39
#define TOUCH_CLK       25

// Touch Coordinate Calibration for 320x240 (Rotation 1)
#define TS_MINX         200
#define TS_MAXX         3700
#define TS_MINY         240
#define TS_MAXY         3800

// ============================================================================
// Firmware & OTA Configuration
// ============================================================================
#define FIRMWARE_VERSION "v1.0.2"
#define GITHUB_REPO      "trongphuc2810/cyd-pixel-clock"

// ============================================================================
// WiFi & Captive Portal Configuration (Zero hardcoded secrets)
// ============================================================================
#define AP_SSID          "CYD-Clock-Setup"
#define AP_PASS          "" // Open captive portal for easy phone setup

// NTP Servers & Timezone (UTC+7 for Vietnam)
#define NTP_SERVER_1     "pool.ntp.org"
#define NTP_SERVER_2     "time.google.com"
#define GMT_OFFSET_SEC   (7 * 3600)
#define DAYLIGHT_OFFSET  0

// ============================================================================
// Color Palette (Pure Black & Crisp White Minimalist Retro)
// ============================================================================
#define COLOR_BG         0x0000 // Pure Black #000000
#define COLOR_WHITE      0xFFFF // Crisp White #FFFFFF
#define COLOR_GRAY_DARK  0x2104 // ~#222222
#define COLOR_GRAY_MID   0x8410 // ~#888888
#define COLOR_GRAY_LIGHT 0xC618 // ~#CCCCCC
#define COLOR_DIVIDER    0x2965 // ~#2A2A2A
