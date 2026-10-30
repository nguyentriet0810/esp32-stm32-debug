# Kiểm tra firmware tích hợp 0.1.0-dev1

Ngày: 30/10/2026. Bản phát triển, chưa phát hành MVP đã nghiệm thu trên STM32.

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
