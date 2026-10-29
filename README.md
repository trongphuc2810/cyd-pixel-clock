# CYD Pixel Clock & Muse Virtual Pet (ESP32-2432S028R)

Firmware đồng hồ pixel phong cách Retro tối giản (Nền đen tuyền, chữ trắng) kết hợp linh vật thú ảo **Muse Pixel** cho board màn hình **CYD (ESP32-2432S028R)** 320x240 cảm ứng.

Hỗ trợ cấu hình WiFi qua Captive Portal điện thoại (không hardcode thông tin cá nhân) và cập nhật OTA tự động qua GitHub Releases.

---

## 1. Tính năng nổi bật

- **Retro Minimalist UI:** Nền đen OLED-like (`#000000`), mặt đồng hồ số trắng (`#FFFFFF`) hiển thị `HH:MM:SS`, thứ viết tắt tiếng Anh (`THU`), ngày tháng chuẩn ISO (`2026-10-29`).
- **Linh vật Muse Pixel Engine:**
  - 100% đồ họa toán học Procedural Q12 (không tốn flash lưu ảnh/GIF).
  - Tự thở nhẹ nhàng, chớp mắt, tỏa sao.
  - **Tương tác cảm ứng:**
    - Chạm vào Muse: Thể hiện cảm xúc lắng nghe (`LISTEN`) hoặc thích thú (`HAPPY`).
    - Nút `[PHONE]`: Chuyển Muse sang chế độ lướt điện thoại/gõ máy phát sáng.
    - Nút `[PET]`: Quay về trạng thái đồng hành thông thường (`IDLE`).
    - Sau 60s không chạm: Tự đổi sang trạng thái chán (`BORED` / thở dài).
    - Sau 180s không chạm: Tự chìm vào giấc ngủ (`SLEEP` / Zzz). Chạm vào bất kỳ đâu sẽ đánh thức Muse dậy ngay lập tức.
- **Bảo mật tuyệt đối (Zero Leaks):**
  - Không hardcode IP, không hardcode SSID / mật khẩu WiFi cá nhân.
  - Tự động phát Hotspot Captive Portal (`CYD-Clock-Setup`) khi chưa có WiFi hoặc khi nhấn giữ nút BOOT lúc khởi động.
- **Tự động cập nhật OTA qua GitHub:**
  - Kiểm tra phiên bản mới từ GitHub Releases qua WiFi.
  - Phân vùng kép `min_spiffs` (1.9MB OTA A/B) có cơ chế an toàn rollback chống brick.

---

## 2. Phần cứng tương thích (CYD)

| Thành phần | Thông số / GPIO |
| :--- | :--- |
| **Board** | ESP32-2432S028R ("Cheap Yellow Display" - CYD 2.8") |
| **Màn hình** | 320x240 ST7789 / ILI9341 (Landscape rotation 1) |
| **Cảm ứng** | XPT2046 Resistive Touch (`CLK: 25`, `MISO: 39`, `MOSI: 32`, `CS: 33`, `IRQ: 36`) |
| **Đèn nền** | GPIO 21 (PWM) |
| **Nút BOOT** | GPIO 0 (Nhấn giữ lúc bật nguồn để ép vào chế độ cấu hình WiFi) |

---

## 3. Hướng dẫn sử dụng

### Cấu hình WiFi lần đầu
1. Cấp nguồn cho CYD. Nếu chưa lưu WiFi, màn hình sẽ hiển thị bảng hướng dẫn:
   - **Hotspot SSID:** `CYD-Clock-Setup`
   - **IP cấu hình:** `192.168.4.1`
2. Dùng điện thoại kết nối vào WiFi `CYD-Clock-Setup`.
3. Trang cấu hình WiFi sẽ tự động bật lên (hoặc mở trình duyệt vào `192.168.4.1`).
4. Chọn mạng WiFi nhà bạn, nhập mật khẩu và nhấn **Save**. CYD sẽ lưu vào flash và tự động kết nối, đồng bộ giờ NTP.

### Cập nhật qua GitHub OTA
- Khi bạn push tag mới lên GitHub (ví dụ `v1.0.1`), GitHub Actions sẽ tự động build file `firmware.bin` và đưa vào Release.
- CYD khi kết nối WiFi sẽ tự nhận diện có bản cập nhật mới, hiển thị màn hình tải và tự nạp rồi khởi động lại.
