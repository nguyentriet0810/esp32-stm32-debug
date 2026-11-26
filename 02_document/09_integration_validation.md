# Kiểm tra firmware tích hợp 0.1.0-dev1

Ngày: 30/10/2026. Bản phát triển, chưa phát hành MVP đã nghiệm thu trên STM32.

## Bring-up Wi-Fi/SWD với STM32F103C8T6 — 26/11/2026

PCB ESP32-S3 N16R8 dùng USB COM9 và STA; người dùng xác nhận STM32 có nguồn 3,3 V,
GND chung, PA13/SWDIO → GPIO4 và PA14/SWCLK → GPIO5. Chưa nối nRST/SWO.

- TCP 2022: `qSupported`, monitor command, ACK/no-ACK và đóng phiên hoạt động;
  snapshot sau mỗi phiên trả `gdb=IDLE`, `target=NONE`, `errors=0`.
- Scan ở 400 kHz, 100 kHz và 10 kHz chưa nhận target. Giữ RESET trên STM32 vẫn chưa nhận.
  Trace đọc `ACK=7`, `data=0xFFFFFFFF`, parity lỗi, không phải DPIDR hợp lệ.
- Kiểm tra mức pad trong chuỗi reset: SWCLK đọc `low=1 high=1` thay vì `0/1`.
  Thử xuất GPIO thường cũng đọc `1/1`; dedicated output latch đúng `0x02`, task ở core 1,
  mux GPIO5→core1 output0 và GPIO4→core1 output1, input/output bật, open-drain tắt.
- Đã thêm khởi tạo pad bằng `gpio_config` (giải phóng RTC) và nhả hold cho GPIO4/5.
  Hiện tượng vẫn còn sau reset/nạp lại và sau khi người dùng tháo riêng SWCLK khỏi STM32.
  Người dùng đo điện áp CLK là 3,3 V và ban đầu báo GPIO5↔3V3 là 0 Ω;
  sau đó xác nhận phép đo thông mạch không thông với GND hoặc 3V3, nên không xác nhận chập cứng.
  R18 thực lắp 22 Ω (schematic ghi 330 Ω). Cuối phiên đã tìm ra linh kiện thực lắp là
  BAT54C thay cho BAT54S; kết quả sau tháo diode được ghi bên dưới.
  Net SWCLK nối U1.5, R18.2, D6.3; D6.2 nối 3V3, J4.3 qua R18.1.
  DRC KiCad 10 trên file AI_layout đã lưu, refill zones trong bộ nhớ: 0 violations,
  0 unconnected; không sửa/lưu board. Báo cáo cục bộ `drc-ai.json`.
- Đã thêm `CONFIG_PROBE_SWD_TRACE` mặc định tắt: lấy mẫu pad trong tám chuỗi output đầu,
  log tối đa tám lần đọc ACK/data/parity. Không log bên trong vòng tạo cạnh, không đổi mux
  hoặc thêm xung thử. Bản thử đổi mux/xung ngắn chỉ dùng cục bộ để khoanh vùng và đã bỏ khỏi source.
- Có `tools/test_stm32_debug.py` làm tiêu chí scan/attach/identity/register/memory và tùy chọn
  step/hardware breakpoint/run-interrupt. Lần đầu chưa qua scan; sau tháo diode và nối lại
  target đã chạy đạt như mục xác nhận bên dưới. Không ghi/xóa flash STM32, không thay option
  bytes hoặc ghi thử RAM.
- Build Debug bình thường (trace tắt): đạt với warnings/analyzer của dự án; app 1.068.608 byte.
  Nhánh trace của port cuối cũng biên dịch đạt bằng cùng toolchain/cờ cảnh báo/analyzer.
  Format C, kiểm tra whitespace, cú pháp Python và cấu hình attach JSON: đạt.
  Binary thường đã build và sau đó nạp lại thành công ở bước trở về firmware chính bên dưới.

### Thử OLED độc lập trên GPIO4/5

Theo yêu cầu chạy thử của người dùng, đã build/nạp ứng dụng riêng `oled_gpio45_test`
qua COM9, chỉ ghi app tại `0x20000`; không thay bootloader, partition table, NVS hay STM32.
App và script thử nằm cục bộ trong `01_source/.cache/oled-gpio45/`, không đổi pin OLED
8/9 hoặc bật/tắt dịch vụ trong mã firmware chính. App thử không khởi động SWD, Wi-Fi, SD.

Trước OLED, app cấu hình input/output open-drain, nhả hold, dùng drive capability thấp nhất,
thử một xung thấp 5 µs mỗi chân rồi nhả. Kết quả runtime:

```text
GPIO4 open-drain: low=1 released=1
GPIO5 open-drain: low=1 released=1
PIN_TEST_FAILED SDA=ESP_ERR_INVALID_STATE SCL=ESP_ERR_INVALID_STATE; OLED not started
```

Cả hai chân chưa kéo thấp được trong phép thử này. Không suy ra riêng ESP32 đã hỏng;
cần cô lập tải/diode và đường PCB. OLED chưa khởi động, chưa có frame hiển thị thành công.
App dừng tạo cạnh sau lỗi, nhả hai chân và chỉ lặp lại log `TEST_STOPPED` mỗi 2 giây.
Log lần đầu được giữ tại `.cache/oled-gpio45/runtime-first-check.log`; esptool xác nhận
hash ghi flash đạt.

Phép đối chứng tiếp theo dùng drive capability mặc định 2, tắt sleep selection trên chân:
GPIO8/9 đều đọc `low=0 released=1`, còn GPIO4/5 vẫn `1/1` dù latch=0, output enable=1,
matrix=256 (GPIO thường). Sau đó app thử I2C thật trên GPIO4/5: probe 0x3C/0x3D timeout,
OLED chưa khởi tạo được. SWD/Wi-Fi vẫn tắt. Cảnh báo reserve GPIO trong phép thử này
phát sinh do bước kiểm tra GPIO trước I2C; SDK chỉ cảnh báo và vẫn cấu hình bus.

### Xác nhận sau tháo diode lắp sai loại

Người dùng phát hiện PCB lắp BAT54C, trong khi schematic yêu cầu BAT54S, rồi tháo các
diode trên SWDIO, SWCLK, SWO và nRST để trống. Theo datasheet, BAT54C có chân 2 là anode
và chân 3 là cathode chung; trên PCB chân 2 nối 3V3, chân 3 nối tín hiệu. Cách lắp này
tạo đường dẫn thuận từ 3V3 tới tín hiệu, phù hợp với hiện tượng bị giữ cao.
BAT54S có chân 2 là cathode và chân 3 là điểm nối hai diode, đúng mạch kẹp của schematic.
Tham chiếu: [BAT54C](https://assets.nexperia.com/documents/data-sheet/BAT54C.pdf),
[BAT54S](https://assets.nexperia.com/documents/data-sheet/BAT54S.pdf).

Sau tháo diode, OLED vẫn nối SDA=GPIO4, SCL=GPIO5; STM32 đã tháo hai dây SWD.
Đọc COM9 không reset thu được:

```text
OLED_FRAME_OK 86
OLED_FRAME_OK 87
OLED_FRAME_OK 88
```

Đã xác nhận truyền frame SH1106 qua I2C 100 kHz trên GPIO4/5 hoạt động sau tháo diode.
Không có cơ sở kết luận hai GPIO này hỏng từ các phép thử trước. Người dùng báo các đường
SWO/nRST cũng hoạt động bình thường; chưa có phép đo/điều khiển riêng hai chân đó trong log.
Log xác nhận: `.cache/oled-gpio45/runtime-after-diode-removal.log`.

Sau bước này PCB tạm chạy app thử OLED; kết quả I2C chưa xác nhận SWD. Sau đó người dùng
đã chuyển OLED về GPIO8/9 (hoặc rời GPIO4/5) và nối lại SWD STM32 để thử firmware chính.

Log/artifact chẩn đoán nằm cục bộ trong `01_source/.cache/swd-f103-20261126/`.

### Trở về firmware chính và xác nhận Wi-Fi/SWD

Đã build/nạp `build-vscode/stm32_wifi_probe.bin` bằng `dev.ps1 -p COM9 -b 460800 app-flash`.
Esptool xác nhận hash ghi đạt; chỉ thay app, không ghi bootloader/partition table/NVS.
Firmware chính chạy lại với trace SWD tắt, OLED trên GPIO8/9 ONLINE, microSD MOUNTED,
STA tự nối từ profile đã lưu. Endpoint trong lần thử: `192.168.137.140:2022`.

| Phép thử | Kết quả |
|---|---|
| Scan SWD 400 kHz, `connect_rst disable` | Target 1: `STM32F1 L/M density M3` |
| CPUID / DBGMCU_IDCODE | `0x411FC231` / `0x00006410` |
| Flash-size signature | 64 KiB (`0x0040` tại `0x1FFFF7E0`) |
| Thanh ghi | Đọc 80 byte, PC/SP/xPSR hợp lệ qua RSP và ARM GDB |
| Đọc bộ nhớ | 10 lần lặp 64 byte đầu flash cho cùng kết quả; đọc RAM thành công |
| Single-step | Đạt, cả script RSP và ARM GDB |
| Hardware breakpoint | Set → continue → dừng đúng PC → clear: đạt |
| Run/interrupt | Continue → Ctrl-C → dừng CPU: đạt |
| Detach/đóng phiên | Trả về IDLE, `errors=0`, ứng dụng STM32 được resume |
| ARM GDB thực tế | Attach, XML/registers, memory map, CPUID/DBGMCU/flash-size/vector reads và stepi/detach đạt |

Snapshot cuối:

```text
STATUS wifi=STA ssid=TRIET ip=192.168.137.140 gdb=IDLE target=NONE swd=400000 oled=ONLINE errors=0 heap=201356 sd=MOUNTED
```

ARM GDB mặc định chặn địa chỉ thanh ghi ngoài memory map flash/RAM. Đã thử thành công
với `set mem inaccessible-by-default off` và thêm vào mẫu Cortex-Debug `preAttachCommands`.
Lần thử GDB chưa chỉ định ELF nên có cảnh báo không có executable/source symbols; không
ảnh hưởng các phép đọc thanh ghi/bộ nhớ và step theo lệnh đã kiểm tra.

BMD upstream hiện gán memory map flash 128 KiB cho nhóm F1 medium-density, trong khi
signature trên target báo 64 KiB; RAM map là 20 KiB. Chưa thay upstream hoặc nghiệm thu
khả năng ghi vùng flash vượt dung lượng báo cáo. Lần thử này không ghi flash STM32,
không thay option bytes, không ghi thử dữ liệu RAM và không dùng nRST/SWO.
Chưa nghiệm thu flash/load/verify, reset phần cứng hoặc debug theo source/Watch với ELF khớp.

Log cục bộ: `01_source/.cache/swd-f103-20261126-after-diodes/` gồm `restore-probe.log`,
`read-rsp.log`, `core-rsp.log`, `arm-gdb-rsp.log` và console tương ứng.
**PCB hiện chạy firmware probe chính**, không còn app thử OLED GPIO4/5.

### Nạp STM32F103C8: PC13 chu kỳ 500 ms

Người dùng yêu cầu nạp ứng dụng LED PC13. Đã tạo source độc lập tại
`01_source/examples/stm32f103_pc13_blink/`, build bằng ARM GCC 13.3.1, C17,
`-Og -g3`, warnings-as-errors và analyzer. Linker giới hạn 64 KiB flash / 20 KiB RAM.
App dùng HSI 8 MHz, SysTick 1 ms; PC13 active-low, sáng 250 ms / tắt 250 ms.

Trước ghi đã backup 64 KiB flash: `.cache/stm32-pc13-20261126/stm32-before-blink-64k.bin`,
SHA256 `69f11392464109a35366685326c4c413ca02ed08362f3a6f80086f35da5bf745`.
Backup và artifact giữ cục bộ, không đưa vào Git.

- `load` qua ARM GDB/Wi-Fi/SWD 400 kHz: đạt, 624 byte flash và 28 byte BSS.
- `compare-sections`: vector và text đều matched; dump 624 byte sau ghi khớp BIN byte-exact.
- `hbreak *main` và `run`: dừng đúng source `main.c` tại `main`; reset không cần dây nRST.
- Script kiểm tra ban đầu lỗi phép toán bit trên `$pc` dạng địa chỉ sau khi đã ghi/verify
  thành công. Đã sửa cast trong script; không nạp lại flash. Bài xác nhận runtime riêng đạt.
- Runtime: `blink_ready=1`, `blink_error=0`, uptime 62340 ms, 249 cạnh, cạnh cuối 62250 ms,
  LED on và GPIOC ODR13=0. SysTick reload=7999, CRH13=2,
  `blink_last_edge_ms == blink_edges * 250`.
- Người dùng xác nhận LED nháy đều. GDB detach, chương trình tiếp tục chạy;
  probe `gdb=IDLE`, `errors=0`, OLED ONLINE và SD MOUNTED.

Đây là lần nạp STM32 đã được người dùng yêu cầu rõ ràng. Không thay option bytes,
không dùng mass erase, không ghi flash/NVS ESP32 trong phép thử này.
Log: `01_source/.cache/stm32-pc13-20261126/{backup,flash,verify}-gdb.log` và console tương ứng.
Mã mẫu/ELF giúp tiếp tục debug source/biến; giao diện VS Code vẫn chưa được chạy để nghiệm thu.

## Tích hợp microSD — 15/11/2026

Đã thêm `probe_sd` trên SPI2, GPIO CS/MOSI/SCK/MISO/DETECT = 38/39/40/41/42,
clock tối đa mặc định 4 MHz. Startup thử mount FAT tại `/sdcard`; các lệnh console
`sd`, `sd mount`, `sd ls`, `sd read`, `sd write`, `sd unmount` gọi API tuần tự hóa.
Lệnh file giới hạn basename 8.3, I/O 256 byte/lần; ghi tạo file mới độc quyền và kiểm tra
fsync/close. Không có format hoặc ghi file thử khi khởi động.

- Build cấu hình SD bật bằng toolchain Xtensa/ESP-IDF v6.1: đạt, GNU17 và GCC analyzer.
- Build cấu hình SD tắt độc lập tại `.cache/sd-off`: đạt; cấu hình chính vẫn giữ SD bật.
- `test_board_config.py` với netlist hiện tại và cả cấu hình SD bật/tắt: đạt,
  gồm 14 GPIO và kết nối OLED/debug/SD.
- `test_sd_host.py`: đạt với SD bật và tắt; dùng production source cùng mock SDK/VFS.
  Đã thử thiếu/tháo thẻ, lỗi card init, hết drive slot, lỗi cấp phát/xung đột VFS,
  mount FAT lỗi, cleanup/retry, tên/file trùng, I/O/sync lỗi và mutex timeout.
- Clang-format dry-run trên C/header thay đổi: đạt.
- Binary SD bật: `01_source/build/stm32_wifi_probe.bin`, **985072 byte**,
  SHA256 `0B14310A898F7849AF6930FD7FCFA167770669DA9D903962CC3E53F9FABEA1A7`.

Component quản lý riêng các bước SPI/device/diskio/VFS/FAT để cleanup khi mount lỗi;
không dùng convenience mount của SDK có nhánh cấp phát context muộn thiếu cleanup.
Mock không kiểm chứng nội bộ SDK hoặc phần điện. Chưa nạp binary mới, chưa thử thẻ thật,
nguồn/tín hiệu hoặc hoạt động SD đồng thời với debug trên PCB. Cụm SD đang mang DNP,
cần lắp đầy đủ trước khi thử. Quy trình thao tác và nghiệm thu ở [microSD](11_sd_storage.md).

## Cập nhật cấu hình PCB — 13/11/2026

Firmware hiện dùng duy nhất pin schematic revision C của PCB REV 1.0. Kconfig cố định
GPIO debug 4/5/6/7, OLED 8/9 và encoder 15/16/17; GPIO38–42 vẫn dành riêng cho SD,
chưa có driver thẻ nhớ. Hai schematic `layout_manual` và `AI_layout` có cùng SHA256
`77799A620E7AD52E5E54E8E3EDEDFBC5ED0166C7C95BC58371E13FC0BF7F23E3`.

- Build `idf.ps1 reconfigure build` bằng ESP-IDF v6.1/toolchain cục bộ: đạt.
  Mã dự án được build với strict warnings, `-Werror` và GCC analyzer.
- `test_board_config.py` với netlist KiCad mới xuất và `sdkconfig` sinh ra: đạt;
  đối chiếu 14 GPIO và kết nối J3/J4. Thử cấu hình/netlist sai chân: bị từ chối đúng.
- Kconfig thay các pin đã lưu bằng pin PCB cố định: đạt; OLED/encoder vẫn cho tắt riêng.
- clang-format dry-run trên `probe_board.c`: đạt.
- Binary mới: `01_source/build/stm32_wifi_probe.bin`, **892048 byte**;
  SHA256 `41ACADC8AFC98D778493EDF4231AB09080C94C7F5836052806893FCC6A5237EF`.

Chưa nạp binary mới lên phần cứng hoặc nghiệm thu PCB lắp ráp. Các kết quả runtime
bên dưới thuộc lần thử DevKit ngày 30/10/2026, không phải kết quả thử PCB mới.

## Môi trường và nguồn mã

| Hạng mục | Giá trị |
|---|---|
| Probe | ESP32-S3 N16R8 DevKit, silicon rev 0.2, hai lõi |
| USB-UART | CH343, COM13, console 115200 baud |
| Flash / PSRAM | 16 MiB Quad / 8 MiB Octal; cấu hình 80 MHz |
| ESP-IDF | v6.1, `fff9895c82d744c7237be8847347bdd1b07c6643` |
| Black Magic Debug | v2.1.0, `449730f8bc6f7a41e3d39e47bc03089b37d4656c` |
| Mã port tham khảo | MCUViewer-probe develop, `02755f409634f0874fb087b720d8883ef6f65dd8` |
| Wi-Fi STA | TRIET, DHCP `192.168.137.47` tại thời điểm kiểm tra |
| GDB endpoint | TCP `192.168.137.47:2022` |
| Ngoại vi | Chưa nối STM32, OLED hoặc encoder |

Đã sao lưu toàn bộ flash 16 MiB trước lần nạp probe đầu tiên, ở thư mục cache cục bộ:
`01_source/.cache/backups/com13-before-probe-20261030-retry.bin`.
SHA256: `B1AB4548FDE44A923032D7E29E723D277A36A9102DC2420C2D6234216BC46B1B`.
Lần đọc 921600 baud ban đầu bị lỗi truyền, bản sao đầy đủ lấy lại ở 460800 baud.
Backup, log và script provisioning chứa credential đều bị Git ignore.

## Kết quả

| Bài thử | Kết quả / bằng chứng |
|---|---|
| Build ESP-IDF, warnings và GCC analyzer | Đạt trên mã dự án; backend giữ cấu hình upstream |
| Format mã dự án | clang-format 21.1.8, dry-run với Werror đạt; không format shim/upstream |
| Build tắt OLED và encoder | Đạt; sau đó khôi phục defaults bật cả hai và build lại |
| Nạp firmware COM13 | 460800 baud, esptool xác nhận hash, reset thành công |
| Profile phần cứng | Flash physical/configured 16777216 byte, PSRAM 8388608 byte |
| Thiếu OLED | Log nonfatal, UI đánh dấu OFFLINE, các dịch vụ khác vẫn chạy |
| Kết nối TRIET | WPA2-PSK, DHCP thành công, nhận IP ở bảng trên |
| Giữ cấu hình sau reset | Reset qua RTS, tự nối TRIET từ NVS, không gửi lại mật khẩu |
| RSP chia nhỏ packet | qSupported gửi từng đoạn ba byte, trả lời đúng checksum |
| Checksum sai | Nhận NACK, packet hợp lệ sau đó vẫn xử lý được |
| Một client sở hữu probe | Kết nối thứ hai bị đóng khi client đầu còn hoạt động |
| Scan không có target | Báo SWD scan failed; phiên vẫn trả lời qSupported |
| GDB no-ack | QStartNoAckMode trả OK, qC trả QC1; phiên mới trở lại ACK mode |
| Ngắt TCP / kết nối lại | 20 phiên liên tiếp trên firmware cuối, qSupported đều thành công |
| Clock khi rảnh | Console đổi 400 kHz → 1 MHz, snapshot phản ánh giá trị đã áp dụng |
| Clock khi đang debug | Bị từ chối ESP_ERR_INVALID_STATE, giữ 1 MHz |
| Giải phóng phiên | Sau đóng client, gdb=IDLE; đổi lại 400 kHz thành công |
| Encoder decoder trên PC | Hai chiều, bounce, partial, chuyển sai, state/input lỗi và 10000 mẫu đạt |
| Panic/watchdog | Không thấy trong log runtime được thu; chưa phải bài thử phiên dài |

Trích snapshot cuối bài thử runtime:

```text
STATUS wifi=STA ssid=TRIET ip=192.168.137.47 gdb=IDLE target=NONE swd=400000 oled=OFFLINE errors=0 heap=240596
```

Firmware đã nạp: `01_source/build/stm32_wifi_probe.bin`, 891856 byte.
SHA256: `B9052BEDCCDE9DCE4BA9C6BE7E7066E28F331CE241D8986B1F48ECCDB1F2A4ED`.
Đây là hash artifact cục bộ của lần kiểm tra này; timestamp build có thể làm hash khác khi build lại.

Log cục bộ: `.cache/flash-final-com13.log`, `.cache/runtime-final-com13.log`, tính từ `01_source`.
Không đưa log/firmware binary hoặc thông tin đăng nhập vào Git.

## Chạy lại các bài thử

Từ root repository, probe đã được cấu hình Wi-Fi và không có target nối dây:

```powershell
. .\01_source\tools\environment.ps1
& $probePython .\01_source\tools\test_probe_runtime.py --serial COM13 --ssid TRIET --reboot --log .\01_source\.cache\runtime-final-com13.log
& $probePython .\01_source\tools\test_gdb_transport.py 192.168.137.47 --expect-no-target
```

Thay IP/cổng serial theo thiết bị thực tế; DHCP có thể cấp IP khác. Đóng serial monitor trước
khi chạy bài thử runtime. Script runtime reset board, đổi clock rồi khôi phục 400 kHz.
Script transport scan SWD, không erase/program target. Test decoder độc lập dùng GCC host:

```powershell
gcc -std=c11 -Wall -Wextra -Werror -Wconversion -Wsign-conversion -pedantic -I 01_source/components/probe_encoder/include 01_source/components/probe_encoder/probe_encoder_decode.c 01_source/tests/encoder_test.c -o 01_source/.cache/encoder_test.exe
.\01_source\.cache\encoder_test.exe
```

Host hiện có GCC 6.3, test dùng C11 subset; compiler target vẫn C17/GNU17 như quy chuẩn.

## Phạm vi còn cần nghiệm thu

- STM32F103/F411/H503/C562: chưa có model nào được xác nhận scan/attach/RAM/flash/debug thực tế.
  Driver được chọn khi build không bảo đảm mọi part number trong một dòng đều dùng được.
- Chưa đo dạng sóng, turnaround, parity và tần số SWD thực tế; clock báo là giá trị danh định.
- Chưa thử OLED/encoder vật lý, tốc độ xoay, số cạnh/nấc, nhiễu và lỗi I2C sau khi đã online.
- Chưa thử mất AP thật, mất target giữa flash, phiên chạy dài hoặc đo tốc độ RAM/flash.
- Chưa có acquisition MCUViewer, SWO capture, JTAG, USB/CMSIS-DAP, VTref/chuyển mức hoặc OTA.
- Menu contrast/timeout và lưu clock NVS chưa triển khai. Mật khẩu Wi-Fi lưu NVS chưa mã hóa.
- Chưa đánh giá MISRA đầy đủ; warnings/analyzer, Doxygen và hồ sơ ngoại lệ không thay checker.

Bước kế tiếp: đấu STM32F103 theo tài liệu phần cứng và nghiệm thu SWD/GDB trên target thật.
