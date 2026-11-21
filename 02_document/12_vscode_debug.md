# Debug STM32 bằng VS Code qua probe ESP32 Wi-Fi

Ngày cập nhật: 16/11/2026. Giao diện debug được chọn là **VS Code + Cortex-Debug**.
Mục tiêu là đặt breakpoint, chạy/dừng, step theo dòng C/C++, xem/sửa biến, thanh ghi,
bộ nhớ và call stack; nạp firmware là một phần của MVP cần nghiệm thu riêng.

Trạng thái: đã có mẫu cấu hình attach, đối chiếu với Cortex-Debug 1.12.1 đang cài trên
máy và backend BMD v2.1.0 của dự án. **Chưa nghiệm thu phiên VS Code với STM32 thật**.
Xem [phạm vi đã kiểm tra](09_integration_validation.md).

## 1. Luồng debug

```text
VS Code / Cortex-Debug
    ↔ ARM GDB trên PC + file ELF STM32
    ↔ TCP/Wi-Fi <probe-ip>:2022
    ↔ ESP32 / Black Magic Debug
    ↔ SWD
    ↔ STM32
```

GDB dùng ELF có debug symbols để ánh xạ tên biến/dòng code sang địa chỉ và kiểu dữ liệu.
ESP32 nhận yêu cầu GDB, thao tác target qua SWD và trả kết quả. VS Code hiển thị kết quả
trong editor, Variables, Watch, Call Stack và các cửa sổ bộ nhớ/thanh ghi.

## 2. Chuẩn bị

- Mở thư mục **ứng dụng STM32** trong VS Code. Repo này chứa firmware probe ESP32;
  file `01_source/build/stm32_wifi_probe.elf` không phải ELF cần dùng để debug STM32.
- Cài [Cortex-Debug](https://marketplace.visualstudio.com/items?itemName=marus25.cortex-debug).
  Máy hiện tại đã có bản 1.12.1; mẫu không yêu cầu cài lại.
- Cần `arm-none-eabi-gdb`, `arm-none-eabi-objdump` và `arm-none-eabi-nm` trên PATH.
  Máy hiện tại đã có các công cụ này trong
  `C:/ST/STM32CubeCLT_1.19.0/GNU-tools-for-STM32/bin`.
  Nếu VS Code chưa nhận PATH, mở lại VS Code hoặc thêm `armToolchainPath` trỏ tới
  thư mục `bin` của bộ công cụ ARM trong cấu hình debug.
- Build ứng dụng STM32 có debug symbols (`-g`), ưu tiên `-O0` hoặc `-Og` khi nghiệm thu.
  ELF phải khớp firmware trên target; biến bị tối ưu hóa có thể không xem được.
- Đấu SWDIO, SWCLK, GND và nRST theo [tài liệu phần cứng](05_hardware_and_local_ui.md).
  Target cần có nguồn và mức tín hiệu phù hợp với probe.
- PC truy cập được IP probe: dùng IP từ console `status` khi qua router, hoặc
  `192.168.4.1` khi PC nối AP của probe. Cổng mặc định là 2022.
- Đóng các phiên ARM GDB/IDE khác trước khi kết nối; probe chỉ nhận một client.

## 3. Cấu hình attach trong project STM32

Sao chép [launch.json mẫu](examples/vscode/launch.json) vào `.vscode/launch.json`
của **project STM32**. Nếu đã có file này, ghép configuration và inputs vào file hiện có.

Trong Run and Debug, chọn `STM32: Attach via ESP32 Wi-Fi`, nhấn F5 và nhập:

1. Đường dẫn ELF tính từ thư mục project đang mở, ví dụ `Debug/my_application.elf`
   hoặc `build/firmware.elf`. Dùng dấu `/` trong đường dẫn.
2. IP và cổng probe, ví dụ `192.168.4.1:2022`.

Cortex-Debug tự kết nối bằng `target extended-remote`. Sau đó mẫu chạy:

```text
monitor swd_scan
attach 1
```

Mẫu dành cho một STM32 được scan thành target số 1. Scan phải nhận được target;
nếu scan thất bại thì dừng và kiểm tra SWD/nguồn, không coi việc mở được TCP là debug thành công.
`attach 1` dừng CPU để bắt đầu quan sát; mẫu không gửi `load` hoặc lệnh reset khi kết nối.
Ứng dụng STM32 cần được nạp trước bằng quy trình đã kiểm chứng.

`servertype` dùng `external` vì GDB server chạy trên ESP32 và được truy cập qua TCP.
`overrideAttachCommands` thay lệnh `monitor halt` mặc định bằng trình tự BMD.
Không thêm một lệnh `target extended-remote` nữa trong danh sách này.

## 4. Thao tác và tiêu chí nghiệm thu

| Thao tác | VS Code | Kết quả cần xác nhận trên target |
|---|---|---|
| Đặt breakpoint | Nhấp lề editor hoặc F9 | Dừng đúng dòng thực thi |
| Chạy tiếp / dừng | F5 / Pause | CPU đổi trạng thái đúng |
| Step Over / Into / Out | F10 / F11 / Shift+F11 | Code và call stack cập nhật đúng |
| Xem/sửa biến | Variables / Watch | Giá trị khớp bộ nhớ và kiểu trong ELF khi CPU dừng |
| Xem stack/thanh ghi/bộ nhớ | Call Stack và các view của debugger | Nội dung phù hợp với vị trí dừng |

Nghiệm thu đầu tiên trên F103, sau đó F411 theo [kế hoạch](04_development_plan.md).
Dùng ứng dụng thử có biến đếm và lời gọi hàm để đối chiếu breakpoint, step và Watch.
Số breakpoint/watchpoint phần cứng tùy MCU.

Mẫu này tập trung vào attach và thao tác debug. **Chưa dùng nút Reset/Restart để nghiệm thu**:
loại `external` mặc định gửi `monitor reset halt` theo OpenOCD; lệnh đó không phù hợp
với hành vi reset của BMD. Cần cấu hình và thử trình tự reset riêng trên target trước
khi thêm profile Reset/Restart và Flash + Debug.

Nạp flash qua `load`, kiểm tra bằng `compare-sections`, reset và dừng tại `main` là
các tiêu chí tiếp theo của MVP, phụ thuộc driver đúng model STM32. Không suy ra đã đạt
các tiêu chí này từ việc cấu hình attach hợp lệ.

SWO/SWV, RTT, Live Watch liên tục và RTOS task awareness chưa được cấu hình/nghiệm thu
cho probe này. Watch khi CPU dừng thuộc luồng debug nền; thu mẫu liên tục là phần mở rộng.

## 5. Chẩn đoán nhanh

| Hiện tượng | Kiểm tra |
|---|---|
| Không tìm thấy GDB/objdump/nm | PATH hoặc `armToolchainPath`; dùng công cụ ARM, không dùng Xtensa GDB của ESP32 |
| TCP không kết nối được | IP thật, mạng PC, cổng 2022, log `GDB ready` và client đang giữ probe |
| TCP được nhưng scan lỗi | Nguồn/GND, SWDIO/SWCLK, pin map, clock SWD và model target |
| Không hiện source hoặc biến | ELF đúng bản, debug symbols, mức tối ưu hóa và đường dẫn source |
| Cần xem lệnh trao đổi | Thêm `"showDevDebugOutput": "raw"` vào configuration rồi xem Debug Console |

## 6. Tham chiếu

- [Cortex-Debug: external GDB server](https://github.com/Marus/cortex-debug/wiki/External-gdb-server-configuration).
- [Thuộc tính cấu hình Cortex-Debug](https://github.com/Marus/cortex-debug/blob/v1.12.1/debug_attributes.md).
- [Trình tự kết nối/attach của external controller 1.12.1](https://github.com/Marus/cortex-debug/blob/v1.12.1/src/external.ts).
- [Logic GDB và SWD của dự án](10_project_logic.md).
