# Tổng quan dự án

## Mục tiêu

Xây dựng probe debug đa dụng cho STM32, có thể debug qua Wi-Fi và mở rộng sang USB.
Kiến trúc tham khảo: [MCUViewer-probe](https://github.com/klonyyy/MCUViewer-probe),
sử dụng Black Magic Debug làm lõi xử lý target ở các mốc tiếp theo.

## Nền tảng đã thống nhất

| Thành phần | Lựa chọn |
|---|---|
| Probe MCU | ESP32-S3 N16R8 |
| Flash | 16 MB Quad SPI, 80 MHz |
| PSRAM | 8 MB Octal SPI, 80 MHz |
| CPU | 240 MHz, hai lõi |
| Framework | ESP-IDF v6.1, commit được ghim bằng submodule |
| Giao tiếp target dự kiến | SWD; SWO và nRST bổ sung theo nhu cầu |
| Host phát triển hiện tại | Windows x64 |

## Phân chia thư mục

- `01_source`: firmware, cấu hình build, script và toàn bộ bộ công cụ cục bộ.
- `02_document`: yêu cầu, quyết định kỹ thuật, hướng dẫn và quy trình làm việc.
- `README.md`: điểm bắt đầu của dự án.

Các thành phần firmware dùng chung sẽ được tách vào `01_source/components` khi có
triển khai thực tế. Không tạo các module rỗng chỉ để hoàn thiện sơ đồ thư mục.

## Các mốc phát triển

Kế hoạch chi tiết và điều kiện nghiệm thu nằm trong
[Kế hoạch phát triển](04_development_plan.md), hiện là bản đề xuất để thống nhất.

- MVP v0.1: debug/nạp flash qua GDB và Wi-Fi, kiểm chứng trên các model STM32 đã chọn.
- Bản v0.2: đọc RAM liên tục và giao thức MCUViewer.
- Các mốc sau: SWO, USB/CMSIS-DAP và phần cứng mở rộng theo nhu cầu.

Firmware nền đã build thành công; chưa nạp board để xác nhận runtime.
Các chức năng debug và trace chưa triển khai. Danh sách STM32 hỗ trợ sẽ được kiểm tra
theo driver và mã chip cụ thể.

## Nguồn tham khảo và giấy phép

Project nền hiện chưa sao chép mã firmware của MCUViewer-probe hoặc Black Magic Debug.
Khi tích hợp, cần giữ thông tin tác giả và tuân thủ giấy phép của từng dependency.
ESP-IDF giữ nguyên giấy phép upstream trong submodule.
