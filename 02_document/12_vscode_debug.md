# Debug STM32 bằng VS Code qua probe ESP32 Wi-Fi

Ngày cập nhật: 26/11/2026. Project giữ debugger **Cortex-Debug hoặc STM32CubeIDE for VS Code**.
Mục tiêu là đặt breakpoint, chạy/dừng, step theo dòng C/C++, xem/sửa biến, thanh ghi,
bộ nhớ và call stack. Các ví dụ cấu hình thủ công bên dưới dùng Cortex-Debug.

Trạng thái: mẫu attach đã đối chiếu với Cortex-Debug 1.12.1 và BMD v2.1.0. Đã thử
ARM GDB thật qua Wi-Fi/SWD trên STM32F103C8T6; scan/attach, đọc thanh ghi/bộ nhớ,
single-step, hardware breakpoint và run/interrupt đạt ở 400 kHz. **MAGIC v0.2.0 đã đạt
F5 attach/launch với cả Cortex-Debug và ST Generic GDB Server** trên firmware PC13 mẫu:
build một lần, Watch `blink_ready=1`, source/call stack, breakpoint, Step Over/Into/Out,
reset và disconnect. Launch F103C8 đã backup đủ 64 KiB, nạp và đọc lại so byte;
profile ST-only không yêu cầu cài Cortex-Debug. Phạm vi và phiên bản kiểm chứng ghi
trong [README extension](../04_extension_for_vscode/README.md#kiểm-chứng).
Xem [phạm vi đã kiểm tra](09_integration_validation.md).

Ngày 26/11 đã tìm ra BAT54C lắp nhầm vị trí BAT54S làm kẹp tín hiệu lên cao. Sau tháo
diode và nối lại STM32, đã nạp firmware probe chính qua COM9; OLED trở lại GPIO8/9,
Wi-Fi và microSD hoạt động. Endpoint STA trong lần thử: `192.168.137.140:2022`;
dùng `status` để lấy IP hiện tại vì DHCP có thể đổi. Sau các bài thử chỉ đọc, người dùng
đã yêu cầu nạp [ví dụ PC13 chu kỳ 500 ms](../01_source/examples/stm32f103_pc13_blink/):
load/compare-sections, readback byte-exact, reset phần mềm và dừng tại `main` với ELF
đều đạt. Biến runtime đọc được bằng tên; người dùng xác nhận LED nháy đều.

## 1. Luồng debug

```text
VS Code / Cortex-Debug hoặc bộ debug ST
    ↔ ARM GDB trên PC + file ELF STM32
    ↔ MAGIC gateway cục bộ (khi dùng extension)
    ↔ TCP/Wi-Fi <probe-ip>:2022
    ↔ ESP32 / Black Magic Debug
    ↔ SWD
    ↔ STM32
```

GDB dùng ELF có debug symbols để ánh xạ tên biến/dòng code sang địa chỉ và kiểu dữ liệu.
ESP32 nhận yêu cầu GDB, thao tác target qua SWD và trả kết quả. VS Code hiển thị kết quả
trong editor, Variables, Watch, Call Stack và các cửa sổ bộ nhớ/thanh ghi.

## 2. Chuẩn bị

Đã có [extension MAGIC Debugger Tool](../04_extension_for_vscode/README.md) để cấu hình
endpoint và tích hợp probe vào cấu hình debug của project. Dùng **Use MAGIC Wi-Fi
Probe in Project**, rồi chọn profile MAGIC và bấm F5; ELF/toolchain/build lấy từ project.
Phần `launch.json` bên dưới vẫn dùng được khi muốn tự cấu hình Cortex-Debug.
Mã các thao tác PC được phát triển trong
`04_extension_for_vscode`, độc lập với firmware ESP32 ở `01_source`.

- Mở thư mục **ứng dụng STM32** trong VS Code. Repo này chứa firmware probe ESP32;
  file `01_source/build/stm32_wifi_probe.elf` không phải ELF cần dùng để debug STM32.
- Nếu dùng mẫu Cortex-Debug thủ công bên dưới, cài [Cortex-Debug](https://marketplace.visualstudio.com/items?itemName=marus25.cortex-debug).
  Máy hiện tại đã có bản 1.12.1; mẫu không yêu cầu cài lại.
- Cần `arm-none-eabi-gdb`, `arm-none-eabi-objdump` và `arm-none-eabi-nm` trên PATH.
  Máy hiện tại đã có các công cụ này trong
  `C:/ST/STM32CubeCLT_1.19.0/GNU-tools-for-STM32/bin`.
  Nếu VS Code chưa nhận PATH, mở lại VS Code hoặc thêm `armToolchainPath` trỏ tới
  thư mục `bin` của bộ công cụ ARM trong cấu hình debug.
- Build ứng dụng STM32 có debug symbols (`-g`), ưu tiên `-O0` hoặc `-Og` khi nghiệm thu.
  ELF phải khớp firmware trên target; biến bị tối ưu hóa có thể không xem được.
- Đấu SWDIO, SWCLK và GND theo [tài liệu phần cứng](05_hardware_and_local_ui.md).
  Với F103C8: PA13 → GPIO4 (J4.2), PA14 → GPIO5 (J4.3), GND → J4.1.
  nRST là dây tùy chọn; mẫu attach dưới đây tắt connect-under-reset để dùng hai dây SWD.
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
set mem inaccessible-by-default off
monitor connect_rst disable
monitor frequency 400000
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

`preAttachCommands` cho phép GDB truy cập địa chỉ ngoài flash/RAM trong memory map,
ví dụ thanh ghi CPUID, DBGMCU và flash-size. Lần thử ARM GDB mặc định chặn đọc
`0xE000ED00`; thêm `set mem inaccessible-by-default off` đã đọc thành công.
Thiết lập này cho phép truy cập, không tự thực hiện đọc hoặc ghi địa chỉ nào.

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

Đã thử `load`, `compare-sections`, readback, reset bằng `run` và hardware breakpoint tại
`main` trên ví dụ F103 PC13. Các model/ứng dụng khác và profile Flash + Debug/Reset trong
VS Code còn cần nghiệm thu riêng; kết quả ARM GDB không tự xác nhận toàn bộ thao tác IDE.

SWO/SWV, RTT, Live Watch liên tục và RTOS task awareness chưa được cấu hình/nghiệm thu
cho probe này. Watch khi CPU dừng thuộc luồng debug nền; thu mẫu liên tục là phần mở rộng.

## 5. Chẩn đoán nhanh

Có thể kiểm tra target mà chưa cần ELF bằng script sau, chạy từ `01_source`:

```powershell
& .\.tooling\python-env\Scripts\python.exe .\tools\test_stm32_debug.py 192.168.4.1
```

Thay IP theo `status`. Script yêu cầu scan thành công, attach target 1, kiểm tra
CPUID/DBGMCU/flash-size, thanh ghi và đọc lặp lại flash/RAM; detach sẽ cho ứng dụng chạy lại.
Không gửi lệnh ghi RAM, erase/program flash hoặc option bytes. Thêm `--exercise-core`
để chạy thêm single-step, breakpoint phần cứng và continue/interrupt trên ứng dụng hiện có;
thao tác này làm ứng dụng tiếp tục thực thi. `--clock 100000` dùng tốc độ thấp khi bring-up.
Ngày 26/11 script đã chạy đạt trên STM32 nối thật với cả chế độ đọc và `--exercise-core`
ở 400 kHz. ARM GDB cũng attach/đọc/single-step/detach thành công, không cần nạp lại STM32.

Khi cần xem phản hồi điện của port, bật `CONFIG_PROBE_SWD_TRACE` trong menuconfig rồi
build/nạp lại ESP32. Console in giới hạn mức pad đọc lại và ACK/data/parity cho mỗi scan.
Mở monitor để thu log; tắt trace khi debug bình thường vì lấy mẫu/log ảnh hưởng timing.
ACK=7 và dữ liệu toàn `1` không phải nhận dạng target hợp lệ. Log mức pad không thay thế
được đo continuity hoặc oscilloscope trên PCB.

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
