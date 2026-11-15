# microSD SPI/FAT

Cập nhật 15/11/2026. Component `probe_sd` cung cấp truy cập thẻ qua SPI2 trên PCB
revision C / REV 1.0. Chưa nghiệm thu trên PCB lắp ráp và thẻ thật.

## Phần cứng và cấu hình

| Tín hiệu | GPIO |
|---|---:|
| CS | 38 |
| MOSI | 39 |
| SCK | 40 |
| MISO | 41 |
| DETECT_N | 42 |

Detect đóng xuống GND khi có thẻ. Driver bật pull-up nội cho detect; PCB có pull-up
ngoài R41. Socket J5 và R33/R34/R36–R41/C15/C16 đang mang cờ DNP trong thiết kế;
cần lắp đủ cụm tương ứng. Pin lấy từ netlist, không từ chú thích SD cũ GPIO14–18.

`CONFIG_PROBE_SD_ENABLE=y` bật mặc định. Clock SPI tối đa mặc định **4 MHz**, chỉnh
`CONFIG_PROBE_SD_MAX_FREQ_KHZ` trong menu `STM32 probe hardware` (400–20000 kHz).
GPIO cố định theo schematic. Sau thay cấu hình, chạy `idf.ps1 reconfigure build`.

Thẻ cần có filesystem FAT hỗ trợ bởi FatFs, ví dụ FAT32. Firmware mount tại `/sdcard`
khi khởi động nếu có thẻ. Thiếu thẻ, lỗi thẻ hoặc filesystem không hợp lệ không dừng
Wi-Fi/GDB/UI. Firmware không format, phân vùng, xóa hoặc ghi file thử khi khởi động.

## Lệnh UART console

| Lệnh | Chức năng |
|---|---|
| `sd` hoặc `sd status` | Detect, trạng thái mount, dung lượng thẻ và lỗi thao tác gần nhất |
| `sd mount` | Thử mount thẻ sau khi cắm; mount đang hoạt động được giữ nguyên |
| `sd unmount` | Đóng mount và giải phóng SPI trước khi tháo thẻ |
| `sd ls` | Liệt kê tối đa 8 mục có tên được hỗ trợ ở thư mục gốc |
| `sd read TEST.TXT` | Đọc tối đa 256 byte đầu file |
| `sd write ...` | Tạo file mới và ghi payload, cú pháp trong ví dụ bên dưới |

Ví dụ gửi nguyên văn trên UART:

```text
sd
sd mount
sd write TEST.TXT|Hello SD
sd read TEST.TXT
sd ls
sd unmount
```

Tên file chỉ nhận chữ ASCII, số, `_`, `-`, stem 1–8 ký tự và phần mở rộng tùy chọn
1–3 ký tự, như `TEST.TXT`. Không nhận đường dẫn hoặc thư mục con. Payload ghi tối đa
256 byte, không tự thêm newline. Lệnh console chỉ nhận payload trên một dòng.

Lệnh ghi dùng tạo mới độc quyền; nếu file đã tồn tại, trả `ESP_ERR_INVALID_STATE` và
giữ nguyên file cũ. Sau ghi, driver kiểm tra đồng bộ và đóng file. Nếu ghi lỗi hoặc mất
nguồn, file mới có thể còn một phần dữ liệu; `ESP_OK` chỉ trả sau khi write/fsync/close đạt.

Kết quả `sd read` gồm `SD_READ`, số byte và cờ `truncated`. Dòng `SD_DATA` in ký tự ASCII
hiển thị được; byte điều khiển, byte ngoài ASCII và dấu `\` được in thành `\xNN`.
`sd ls` đánh dấu `truncated=YES` khi còn mục khác hoặc bỏ qua tên ngoài tập hỗ trợ;
driver quét tối đa 32 directory record mỗi lần. `bytes` trong `SD_STATUS` là dung lượng
vật lý của thẻ, không phải số byte trống trong filesystem. Lệnh `status` chung có thêm `sd=`.
API metadata file của SDK dùng kích thước signed 32 bit: file từ 2 GiB trở lên chưa được
hỗ trợ đọc trong bản này; listing bỏ qua metadata không biểu diễn được và đánh dấu truncated.

## Vòng đời và nghiệm thu

Component tuần tự hóa mount/unmount và I/O bằng mutex; chờ mutex tối đa một giây.
Mỗi API chỉ giữ handle file trong chính lời gọi, không chia sẻ descriptor với module khác.
Không dùng VFS `/sdcard` trực tiếp từ component khác; gọi API `probe_sd` để giữ đồng bộ.
Lỗi/mất thẻ được phát hiện khi gọi API; không có task tự mount khi cắm thẻ. Sau tháo/cắm
lại, dùng `sd mount`; luôn `sd unmount` trước khi tháo. Đây chưa phải hỗ trợ hot-swap tự động.

Các bước cần chạy trên PCB thật:

1. Khởi động không thẻ: Wi-Fi/GDB/console vẫn chạy, `sd` báo `present=NO`.
2. Cắm thẻ FAT và `sd mount`: báo `mounted=YES`, dung lượng hợp lý.
3. Với tên file chưa có, ghi chuỗi đã biết, đọc lại và đối chiếu dữ liệu.
4. Ghi lại cùng tên: bị từ chối, dữ liệu cũ còn nguyên.
5. Thử tên không hợp lệ, file thiếu, file lớn hơn 256 byte và thư mục nhiều mục.
6. Unmount/mount lặp, thẻ lỗi/filesystem không hợp lệ và tháo thẻ: không panic;
   phục hồi bằng cắm lại và `sd mount`. Tránh tháo trong lúc ghi.
7. Dùng SD trong khi có phiên GDB để kiểm tra ảnh hưởng nguồn, timing SWD và mạng.

Chức năng nạp STM32 trực tiếp từ file trên thẻ chưa được triển khai trong cập nhật này.

## Kiểm tra trên máy phát triển

```powershell
& .\01_source\.tooling\python\python.exe .\01_source\tools\test_sd_host.py
```

Runner dùng GCC host, có thể chỉ định `--cc <đường-dẫn-gcc>`. Mã production được compile
với mock GPIO/SPI/FAT/VFS/mutex, không truy cập thẻ hoặc filesystem thật. Bài thử bao gồm
thiếu/tháo thẻ, lỗi khởi tạo và cleanup/retry, tên file, file trùng, read/write/sync lỗi,
giới hạn listing và cấu hình tắt SD. Build target vẫn dùng SDK/toolchain Xtensa đã ghim.
Các mock chỉ kiểm tra logic component; nghiệm thu driver SDK, nguồn và tín hiệu cần PCB thật.
