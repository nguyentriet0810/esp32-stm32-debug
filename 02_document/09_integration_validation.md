# Kiểm tra firmware tích hợp 0.1.0-dev1

Ngày: 30/10/2026. Bản phát triển, chưa phát hành MVP đã nghiệm thu trên STM32.

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
