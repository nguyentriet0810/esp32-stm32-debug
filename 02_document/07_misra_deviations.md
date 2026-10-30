# Hồ sơ ngoại lệ và ranh giới MISRA

## Trạng thái

Các hồ sơ dưới đây ghi quyết định kỹ thuật hiện tại và vị trí cần đối chiếu khi phân tích MISRA.
Chúng **chưa phải deviation record MISRA đã hoàn tất**: chưa gắn đầy đủ rule/category từ
bản tiêu chuẩn đã chọn, chưa có kết quả checker và guideline compliance summary.
Không dùng danh sách này để bỏ qua toàn bộ cảnh báo hoặc tuyên bố dự án đã tuân thủ MISRA.

## SDK-001 — Logging qua ESP-IDF

- Phạm vi: mọi lời gọi `ESP_LOGI`/`ESP_LOGE` trong mã C tự viết, hiện tại ở
  `main/main.c` và `components/probe_board/probe_board.c`.
- Lý do: giữ đường chẩn đoán chuẩn của nền tảng. SDK cung cấp logging bằng macro,
  đối số biến thiên và các phần mở rộng của compiler trong header.
- Kiểm soát: chỉ gọi trong task phù hợp; format string cố định; dùng `PRIu32`, `%zu`
  và cast mở rộng an toàn theo đúng kiểu đối số; bật warning format và lỗi kiểu dữ liệu.
- Timing: không log trong đường bit-banging SWD hoặc ISR. Sự kiện cần chẩn đoán
  trên đường đó sẽ được chuyển cho task xử lý.
- Kiểm chứng hiện tại: build compiler và review; đối chiếu rule ID/khả năng phân tích
  phần macro expansion còn cần thực hiện bằng checker đã chọn.

## SDK-002 — Dừng khởi động khi sai profile phần cứng

- Phạm vi: `ESP_ERROR_CHECK(result)` trong `app_main`.
- Lý do: profile N16R8 là điều kiện nền trước khi khởi chạy dịch vụ debug.
- Kiểm soát: `probe_board_check()` trả lỗi, không tự abort/reset. Application quyết định
  dừng khởi động khi kiểm tra thất bại, giữ hành vi của firmware nền ban đầu.
- Giới hạn: không dùng chính sách abort này cho lỗi I2C/OLED, mất mạng hoặc mất target
  trong các dịch vụ có khả năng phục hồi.
- Kiểm chứng hiện tại: build/review; runtime trên board vẫn chưa thực hiện.

## SDK-003 — Kiểu và header tại ranh giới nền tảng

- Phạm vi: API `esp_err_t`, `esp_chip_info_t`, kiểu enum SDK và header ESP-IDF được include
  bởi component adapter hoặc application entry point.
- Lý do: ABI của ESP-IDF được giữ nguyên, còn dữ liệu riêng của dự án dùng kiểu/đơn vị rõ ràng.
- Kiểm soát: public dependency được khai báo rõ; lỗi flash driver được trả nguyên giá trị;
  không tự định nghĩa lại SDK type hoặc đổi build mode của thư viện upstream.
- Mã dự án: biên dịch C17; SDK v6.1 dùng cấu hình GNU23 của upstream. Cờ warning/analyzer
  dự án áp dụng riêng cho target tự viết, không phải chứng nhận mã SDK.
- Việc còn lại: cấu hình phạm vi include/macro/library cho checker, ghi toolchain properties
  cần thiết và lập guideline coverage theo từng rule.

## Mẫu cho hồ sơ bổ sung

Hồ sơ mới cần có: ID; rule/directive và category đã đối chiếu; file/symbol/phạm vi;
lý do và lựa chọn thay thế; điều kiện áp dụng; ảnh hưởng; biện pháp kiểm soát;
bằng chứng kiểm tra; trạng thái review và người chịu trách nhiệm kỹ thuật.
Các trường chưa có bằng chứng phải ghi là chưa hoàn tất.
