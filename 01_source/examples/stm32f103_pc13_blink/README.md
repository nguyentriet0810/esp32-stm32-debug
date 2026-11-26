# STM32F103C8: LED PC13 chu kỳ 500 ms

Ứng dụng chạy **trên STM32**, độc lập với firmware probe ESP32.
PC13 sáng 250 ms, tắt 250 ms với LED active-low. HSI nội 8 MHz cấp clock cho SysTick
1 ms, không cần thạch anh ngoài. Thời gian là giá trị danh định theo HSI.
Chương trình giữ nguyên chức năng SWD trên PA13/PA14.

## Source và build

- `main.c`: clock, PC13 và SysTick; đổi `half_period_ms` để chỉnh thời gian mỗi trạng thái.
- `startup.S`: vector, stack và khởi tạo data/BSS.
- `linker.ld`: giới hạn đúng 64 KiB flash và 20 KiB RAM của C8.
- `build.ps1`: dùng ARM GCC trên PATH hoặc CubeCLT đã cài; chỉ build, không tự nạp.

Từ root repository:

```powershell
& .\01_source\examples\stm32f103_pc13_blink\build.ps1
```

Truyền `-ToolchainBin` để chọn bộ công cụ ARM khác. ELF, BIN, object và map nằm trong
`01_source/.cache/stm32_pc13_blink/`. ELF có debug symbols (`-Og -g3`);
không dùng ELF Xtensa của probe để debug chương trình này.

## Nạp qua probe Wi-Fi/SWD

STM32 PA13 → probe GPIO4, PA14 → GPIO5, nguồn 3,3 V và GND chung. Lấy IP bằng console
`status`; IP trong lần thử là `192.168.137.140`, TCP 2022. Chỉ mở một client GDB.
Các lệnh sau **ghi đè ứng dụng STM32**:

```gdb
file D:/esp32_debug/01_source/.cache/stm32_pc13_blink/pc13_blink.elf
set mem inaccessible-by-default off
target extended-remote 192.168.137.140:2022
monitor connect_rst disable
monitor frequency 400000
monitor swd_scan
attach 1
load
compare-sections
hbreak *main
run
delete breakpoints
detach
```

Trong lần thử, `run` reset target và dừng đúng hardware breakpoint ở `main`; `detach`
cho chương trình tiếp tục chạy. Backend dùng reset phần mềm khi không có dây nRST.
Không dùng `monitor reset halt` của OpenOCD với backend này.

Để debug trong VS Code, dùng [mẫu Cortex-Debug](../../../02_document/examples/vscode/launch.json)
và [hướng dẫn STM32](../../../02_document/12_vscode_debug.md). Nếu mở chính thư mục ví dụ
này, ELF nhập vào mẫu là `../../.cache/stm32_pc13_blink/pc13_blink.elf`.

## Kết quả trên mạch — 26/11/2026

- Build C17 với warnings/analyzer: đạt; 624 byte flash và 28 byte BSS.
- Đã sao lưu 64 KiB ứng dụng cũ trước khi nạp, giữ cục bộ trong
  `../../.cache/stm32-pc13-20261126/stm32-before-blink-64k.bin`.
- Nạp qua ARM GDB → Wi-Fi → ESP32/BMD → SWD; `compare-sections` và readback 624 byte khớp.
- Dừng tại `main` bằng hardware breakpoint, hiển thị được file/dòng source.
- Runtime: `blink_ready=1`, `blink_error=0`, SysTick reload 7999,
  `blink_last_edge_ms == blink_edges * 250`, PC13 output push-pull 2 MHz.
- Snapshot: uptime 62340 ms, 249 cạnh, cạnh cuối 62250 ms, LED on và PC13=0.
- Người dùng xác nhận LED nháy đều. Kết thúc ở trạng thái chạy tự do sau detach.

Các biến `blink_uptime_ms`, `blink_edges`, `blink_led_on`, `blink_last_edge_ms`,
`blink_ready`, `blink_error` có thể xem bằng GDB/Watch khi CPU dừng.
Backup, log và artifact không đưa vào Git; không thay option bytes hoặc flash ESP32
trong phép thử nạp STM32 này.

Tham chiếu thanh ghi/clock: [ST RM0008](https://www.st.com/resource/en/reference_manual/rm0008-.pdf),
[STM32F103x8/xB](https://www.st.com/resource/en/datasheet/stm32f103c8.pdf).
