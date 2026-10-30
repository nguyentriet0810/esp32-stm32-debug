# Hồ sơ ngoại lệ và ranh giới MISRA

## Trạng thái

Các hồ sơ dưới đây ghi quyết định kỹ thuật hiện tại và vị trí cần đối chiếu khi phân tích MISRA.
Chúng **chưa phải deviation record MISRA đã hoàn tất**: chưa gắn đầy đủ rule/category từ
bản tiêu chuẩn đã chọn, chưa có kết quả checker và guideline compliance summary.
Không dùng danh sách này để bỏ qua toàn bộ cảnh báo hoặc tuyên bố dự án đã tuân thủ MISRA.

## SDK-001 — Logging qua ESP-IDF

- Phạm vi: lời gọi logging ESP-IDF trong main và các component adapter tự viết.
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
- Kiểm chứng hiện tại: build/review và khởi động thành công trên ESP32-S3 N16R8 thật.

## SDK-003 — Kiểu và header tại ranh giới nền tảng

- Phạm vi: API `esp_err_t`, `esp_chip_info_t`, kiểu enum SDK và header ESP-IDF được include
  bởi component adapter hoặc application entry point.
- Lý do: ABI của ESP-IDF được giữ nguyên, còn dữ liệu riêng của dự án dùng kiểu/đơn vị rõ ràng.
- Kiểm soát: public dependency được khai báo rõ; lỗi flash driver được trả nguyên giá trị;
  không tự định nghĩa lại SDK type hoặc đổi build mode của thư viện upstream.
- Mã logic: C17, adapter nền tảng: GNU17 theo SDK-004; SDK v6.1 dùng GNU23. Cờ warning/analyzer
  dự án áp dụng riêng cho target tự viết, không phải chứng nhận mã SDK.
- Việc còn lại: cấu hình phạm vi include/macro/library cho checker, ghi toolchain properties
  cần thiết và lập guideline coverage theo từng rule.

## SDK-004 — GNU17 cho adapter Xtensa/FreeRTOS

- Phạm vi: component gọi `probe_apply_c_options(... SDK_ADAPTER)`; không bao gồm upstream BMD.
- Lý do: header Xtensa/FreeRTOS của SDK dùng từ khóa GNU `asm`; strict ISO C17 không parse được.
- Kiểm soát: vẫn là chuẩn nền C17, không chuyển mã tự viết sang GNU23; giữ toàn bộ warnings,
  `-Werror` và `-fanalyzer`. Board/main không cần extension vẫn dùng `-std=c17`.
- Bằng chứng: compile database ghi cờ dialect ở cuối mỗi lệnh; build firmware thành công.
- Logic decoder chỉ dùng C chuẩn, có bài thử host độc lập; host GCC 6.3 chạy C11 subset.

## SDK-005 — Warning conversion trong HAL inline upstream

- Phạm vi: hai include HAL dedicated GPIO/GPIO LL trong `probe_debug/port/probe_swd.c`.
- Lý do: các hàm inline upstream chuyển bitfield thanh ghi phát sinh conversion warning.
- Kiểm soát: `#pragma GCC diagnostic push/pop` chỉ bao hai include đó, chỉ bỏ hai nhóm
  conversion/sign-conversion trong phần header; mã tự viết sau include vẫn giữ kiểm tra.
- Bằng chứng: build với `-Werror` và review phạm vi pragma; chưa đối chiếu rule MISRA đầy đủ.

## BMD-001 — Ranh giới backend Black Magic Debug

- Phạm vi: `TRY/CATCH` trong task SWD owner; ABI/hooks của BMD, libc shim trong `probe_bmd/port`.
- Lý do: backend upstream dùng `setjmp/longjmp` để thoát lỗi giao dịch target và có stdio,
  allocation theo vòng đời target. Giữ cơ chế phục hồi của backend thay vì đổi semantics upstream.
- Kiểm soát: một owner core 1, không dùng non-local jump qua task/lock dự án; handler giải phóng
  danh sách target, đặt lại trạng thái và báo lỗi. Upstream được ghim commit và giữ giấy phép.
- Timing/register port dùng IRAM/GNU inline attribute, GPIO LL và cycle counter 32 bit Xtensa;
  phép đổi modular difference sang `int32_t` dựa trên compiler/ABI đã ghim, deadline nhỏ hơn nửa vòng.
- Bằng chứng hiện có: build, scan không có target, ngắt/kết nối lại transport. Chưa kiểm chứng
  phục hồi lỗi trong lúc flash target thật; chưa có checker/rule mapping MISRA cho backend.

## Mẫu cho hồ sơ bổ sung

Hồ sơ mới cần có: ID; rule/directive và category đã đối chiếu; file/symbol/phạm vi;
lý do và lựa chọn thay thế; điều kiện áp dụng; ảnh hưởng; biện pháp kiểm soát;
bằng chứng kiểm tra; trạng thái review và người chịu trách nhiệm kỹ thuật.
Các trường chưa có bằng chứng phải ghi là chưa hoàn tất.
