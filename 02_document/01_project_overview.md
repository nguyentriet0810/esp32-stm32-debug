# Tổng quan dự án

## Mục tiêu

Xây dựng probe debug đa dụng cho STM32, có thể debug qua Wi-Fi và mở rộng sang USB.
Kiến trúc tham khảo: [MCUViewer-probe](https://github.com/klonyyy/MCUViewer-probe),
sử dụng Black Magic Debug v2.1.0 làm lõi xử lý target, đã ghim bằng submodule.

## Nền tảng đã thống nhất

| Thành phần | Lựa chọn |
|---|---|
| Probe MCU | ESP32-S3 N16R8 |
| Flash | 16 MB Quad SPI, 80 MHz |
| PSRAM | 8 MB Octal SPI, 80 MHz |
| CPU | 240 MHz, hai lõi |
| Framework | ESP-IDF v6.1, commit được ghim bằng submodule |
| Giao tiếp target dự kiến | SWD; SWO và nRST bổ sung theo nhu cầu |
| Màn hình cục bộ | OLED I2C 0,96 inch, SSD1306 128×64 |
| Điều khiển cục bộ | Encoder cơ khí rời A/B/common và nút nhấn, theo ảnh người dùng chọn; hình dáng EC11, chưa xác nhận mã linh kiện |
| Host phát triển hiện tại | Windows x64 |

Board probe hiện có là DevKit ESP32-S3 N16R8 có chân đưa ra sẵn.
Target hiện có: F103, F411, H503 và C562. Kế hoạch đề xuất nghiệm thu nền trên F103/F411,
sau đó mở rộng H503/C562 sau khi kiểm tra driver và mã chip cụ thể.
Giữ các chân debug của dự án mẫu và bổ sung OLED/encoder theo
[thiết kế phần cứng và UI](05_hardware_and_local_ui.md).

## Phân chia thư mục

- `01_source`: firmware, cấu hình build, script và toàn bộ bộ công cụ cục bộ.
- `02_document`: yêu cầu, quyết định kỹ thuật, hướng dẫn và quy trình làm việc.
- `README.md`: điểm bắt đầu của dự án.

`main` giữ entry point và chính sách khởi động. Các component riêng đảm nhiệm board,
backend BMD, SWD/GDB, network, snapshot trạng thái, console, OLED, encoder và menu.
Xem cây thư mục trong README và [quy chuẩn mã nguồn](06_coding_standard.md).

Luồng khởi động, phối hợp task/component, debug và nạp flash theo từng khối được mô tả trong
[Logic hoạt động dự án](10_project_logic.md).

## Các mốc phát triển

Kế hoạch chi tiết và điều kiện nghiệm thu nằm trong
[Kế hoạch phát triển](04_development_plan.md), cập nhật theo kết quả thực hiện.

- MVP v0.1: debug/nạp flash qua GDB và Wi-Fi, OLED/encoder để xem trạng thái và cấu hình probe,
  kiểm chứng trên các model STM32 đã chọn.
- Bản v0.2: đọc RAM liên tục và giao thức MCUViewer.
- Các mốc sau: SWO, USB/CMSIS-DAP và phần cứng mở rộng theo nhu cầu.

Firmware 0.1.0-dev1 đã chạy trên COM13, kết nối Wi-Fi và phục vụ GDB/TCP.
SWD/backend đã tích hợp nhưng chưa nghiệm thu trên target STM32 thật.
OLED/encoder đã có code, chưa kiểm tra với ngoại vi thật. Encoder đã chọn dạng rời theo ảnh.
Trace và MCUViewer
acquisition chưa triển khai. [Báo cáo kiểm tra](09_integration_validation.md) ghi rõ phạm vi.

## Nguồn tham khảo và giấy phép

Mã gốc dự án dùng GPL-3.0-or-later theo quyết định đã xác nhận, giữ hướng tích hợp Black Magic Debug.
Port SWD/platform/GDB transport được điều chỉnh từ MCUViewer-probe; giữ thông tin tác giả.
Black Magic Debug giữ nguyên mã upstream trong submodule.
ESP-IDF giữ nguyên giấy phép upstream trong submodule.
Xem [thông báo dependency](08_third_party_notices.md).
