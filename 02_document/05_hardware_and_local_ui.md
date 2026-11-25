# Phần cứng và giao diện điều khiển tại probe

Trạng thái: **driver SH1106 cập nhật 25/11/2026; pin firmware đã đối chiếu schematic PCB revision C ngày
13/11/2026, chưa nghiệm thu ngoại vi/target thật trên PCB**.

## 1. Yêu cầu đã xác nhận

- Probe: ESP32-S3-WROOM-1-N16R8 trên PCB riêng, ESP-IDF v6.1.
- Pin hiện tại theo [schematic revision C](../03_pcb/layout_manual/schematic.kicad_sch).
  SWDIO/SWCLK/SWO giữ GPIO4/5/6; target nRST chuyển sang GPIO7.
- OLED I2C **SH1106 128×64**; chuyển driver theo yêu cầu ngày 25/11/2026.
- Thêm một rotary encoder để thao tác menu và điều chỉnh thông số khi cần.
- Ngày 30/10/2026, người dùng chọn encoder cơ khí rời có nút nhấn theo ảnh đã cung cấp.
  Hình dáng giống dòng EC11, với nhóm A/B/common và cặp tiếp điểm nút nhấn riêng.
  Ảnh chưa cho thấy mã linh kiện nên chưa xác nhận hãng, model, pinout hoặc số xung/nấc.

## 2. Pin map cố định trong firmware

Các số dưới đây là **số GPIO**, không phải số chân vật lý của module U1 hay header.
Nguồn đối chiếu là netlist xuất trực tiếp từ schematic đang lưu.
Hai schematic `layout_manual/schematic.kicad_sch` và `AI_layout/AI_layout.kicad_sch`
giống nội dung, cùng revision C; PCB `AI_layout` mang nhãn REV 1.0.

| Nhóm | Tín hiệu | ESP32-S3 | Ghi chú |
|---|---|---|---|
| Debug | SWDIO | GPIO4 | Net SWDIO; hai chiều |
| Debug | SWCLK | GPIO5 | Net SWCLK; output |
| Debug | SWO | GPIO6 | Net SWO; dành cho giai đoạn trace, chưa có driver thu |
| Debug | Target nRST | GPIO7 | Net TARGET_NRST; open-drain, không phải EN/reset của S3 |
| OLED | SDA | GPIO8 | Bus I2C riêng cho giao diện |
| OLED | SCL | GPIO9 | Bus I2C riêng cho giao diện |
| Encoder | A / CLK | GPIO15 | Net ENC_A; input quadrature A |
| Encoder | B / DT | GPIO16 | Net ENC_B; input quadrature B |
| Encoder | SW | GPIO17 | Net ENC_SW; input nút nhấn active-low qua R26 |
| microSD | CS_N | GPIO38 | Net SD_CS_N; SPI2 CS |
| microSD | MOSI | GPIO39 | Net SD_MOSI; SPI2 output |
| microSD | SCK | GPIO40 | Net SD_SCK; qua R34 22 Ω |
| microSD | MISO | GPIO41 | Net SD_MISO; SPI2 input |
| microSD | DETECT_N | GPIO42 | Net SD_DETECT_N; active-low, có thẻ kéo xuống GND |
| Board | BOOT/setup | GPIO0 | Nút BOOT SW2; không dùng cho encoder |
| Nguồn | OLED VCC | 3V3 | Module phải hỗ trợ nguồn 3,3 V và mức I2C 3,3 V |
| Nguồn | GND | GND | OLED, encoder, S3 và target dùng chung mốc GND |

GPIO21 không nối trong schematic hiện tại; PCB không dùng LED GPIO21 của board tham khảo.

Pin map này giữ GPIO19/20 cho USB, GPIO43/44 cho UART0 và không dùng các chân strapping
cho linh kiện UI mới. GPIO35/36/37 của module R8 dùng cho Octal PSRAM nên không được
đưa vào nhóm mở rộng. GPIO38–42 thuộc riêng driver microSD; không dùng cho debug/UI.

Firmware chỉ dùng pin PCB này. Kconfig cố định các GPIO; `idf.ps1 reconfigure build`
sinh lại cấu hình đúng ngay cả khi `sdkconfig` đã lưu pin khác. Clock SWD, cổng TCP và
bật/tắt OLED/encoder vẫn cấu hình được. Không có profile pin DevKit riêng.

Driver `probe_sd` đã có SPI/FAT và lệnh đọc/ghi console. Socket và cụm
R33/R34/R36–R41/C15/C16 đang mang cờ DNP, cần lắp đủ cụm để dùng thẻ.
Pin SD trong bảng lấy theo kết nối thật, không theo chú thích cũ GPIO14–18 trong schematic.
Hướng dẫn sử dụng và nghiệm thu: [microSD](11_sd_storage.md).

## 3. Đấu nối OLED và encoder

### OLED SH1106

| Nhãn trên module | Nối tới |
|---|---|
| VCC | 3V3 |
| GND | GND |
| SDA | GPIO8 |
| SCL | GPIO9 |

Thiết kế ban đầu dùng module I2C bốn chân. Nếu module có thêm RESET hoặc cách cấu hình
địa chỉ khác, bổ sung vào cấu hình board trước khi triển khai driver.
Địa chỉ dự kiến là `0x3C`; xác minh bằng I2C probe, cho phép cấu hình `0x3D` khi phần cứng yêu cầu.
Khởi đầu bus ở 100 kHz, đánh giá 400 kHz sau khi kiểm tra module, dây và pull-up.
Schematic có pull-up SDA/SCL 4,7 kΩ về 3,3 V tại R14/R15; tính cả pull-up trên module OLED
khi kiểm tra bus thực tế.

SH1106 có RAM 132×64 nhưng module hiển thị 128×64. Firmware mặc định ghi từ cột RAM 2;
`CONFIG_PROBE_OLED_COLUMN_OFFSET` cho phép chọn 0..4 trong `menuconfig` nếu module dùng
cửa sổ khác. ACK tại `0x3C`/`0x3D` chỉ xác nhận thiết bị I2C, không tự nhận diện loại
controller. Firmware hiện khởi tạo SH1106 trực tiếp, không tự chọn lại SSD1306.
Ảnh xoay 180° theo yêu cầu ngày 25/11/2026 bằng cặp lệnh segment/common `A0/C0`
thay cho hướng module `A1/C8`; bố cục framebuffer và thao tác encoder không đổi.

### Rotary encoder rời đã chọn

Đấu theo chức năng chân sau khi xác nhận pinout của linh kiện thực tế:

| Chức năng chân encoder | Nối tới | Vai trò |
|---|---|---|
| A | GPIO15 | Pha A quadrature, net ENC_A |
| B | GPIO16 | Pha B quadrature, net ENC_B |
| C / common | GND | Chân chung của hai tiếp điểm xoay |
| S1 của công tắc nhấn | GPIO17 qua R26 (1 kΩ) | SW active-low, net ENC_SW |
| S2 của công tắc nhấn | GND | Nhấn sẽ kéo GPIO17 xuống thấp qua R26 |

Đây là sơ đồ theo chức năng, không phải thứ tự chân trái/phải nhìn từ ảnh. Không gán
chân giữa là C hoặc xác định A/B chỉ từ góc chụp. Các tai kim loại lớn dùng để cố định
vỏ không được dùng thay chân common khi chưa kiểm tra thông số linh kiện.

Encoder tiếp điểm rời dùng cách đấu trên không cần chân VCC riêng. A/B/SW được kéo lên
mức logic 3,3 V qua pull-up ngoài 10 kΩ tại R23/R24/R25; nút nhấn có R26/C10 lọc RC.
Driver vẫn bật pull-up nội cho GPIO15/16/17. Kiểm tra waveform và chiều xoay trên PCB
thực tế trước khi nghiệm thu decoder. Không đưa mức 5 V vào GPIO ESP32.

Trước đấu dây, xác định nhóm A/B/C và cặp công tắc theo datasheet hoặc đo thông mạch khi
linh kiện chưa nối nguồn. Cặp nút nhấn phải đóng khi ấn và mở khi thả. Xác nhận các tiếp
điểm A–C, B–C thay đổi khi xoay; sau đấu nối kiểm tra chuỗi A/B và chiều xoay bằng log/phép đo.
Nếu chiều menu ngược, có thể hoán đổi A/B khi đã tắt nguồn.

Driver hiện polling mỗi 1 ms và dùng decoder full-step: bốn cạnh hợp lệ về AB=3 tạo một
sự kiện menu. Nó loại chuyển hai bit đồng thời và bù bước khi dội ngược; nút debounce
20 ms, giữ 750 ms. Đây là giả định của driver hiện tại, chưa phải thông số đã xác nhận
của encoder trong ảnh.

Số nấc cơ khí không nhất thiết bằng số chu kỳ A/B; mức A/B tại vị trí nấc cũng thay đổi
theo model. Tài liệu EC11 của Alps Alpine nêu rõ khác biệt này. Khi thử cần bảo đảm mỗi
nấc tạo đúng một bước menu, cả hai chiều. Nếu phải xoay hai nấc mới có một bước, hoặc
đếm không ổn định tại nấc, cần đối chiếu waveform để điều chỉnh chế độ đếm/vị trí kết thúc
chu kỳ; không mặc định tăng debounce sẽ giải quyết được. Chưa đổi decoder khi chưa có số đo.

Nguồn theo PCB hiện tại: USB J1 hoặc đầu vào được thiết kế tại J4.6/J4.7.
J4.6 không phải VTref và không cách ly hoàn toàn nguồn cấp ngược; khi cấp USB,
không nối đồng thời một nguồn 3,3 V khác vào J4.6. Target dùng nguồn phù hợp riêng.

## 4. Nội dung hiển thị và thao tác

### Menu hiện đã triển khai

| Trang | Nội dung |
|---|---|
| Startup | Tắt → sáng theo đường chéo trái trên đến phải dưới → tắt nền theo cùng hướng, giữ chữ MAGIC / DEBUGGER / TOOL |
| Settings | Header `Settings`, một hàng trống, bốn mục Flash / Target / Link / OLED %, Info ở dòng cuối |
| Flash source | Chọn REMOTE / SDCARD; chốt về màn chính; SDCARD khóa Target và Link |
| Target IF | Chọn SWD / UART / I2C / SPI trong model UI |
| Host link | Chọn WiFi / BLE / ESP-NOW trong model UI |
| OLED | Chọn contrast 10 / 25 / 50 / 75 / 100 %, xem trước trực tiếp trên panel |
| Info | MAGIC DEBUGGER TOOL, (C) 2026 Nguyen Hoang Minh Triet, GPL-3.0-or-later; chỉ đọc |

Màn chính mặc định:

```text
    Settings

>Flash   REMOTE>
 Target     SWD>
 Link      WiFi>
 OLED %      50>

 Info          >
```

Màn 128×64 chia 8 dòng × 16 ký tự, mỗi ô 8×8 pixel. Dùng đúng bảng `font8x8_basic_tr`
do chủ dự án cung cấp, đặt trong `probe_oled/font8x8_basic_tr.h` dưới dạng `static const`.
Bảng 128 glyph chiếm 1 KiB, mỗi byte là một cột, bit 0 ở trên; có mũi tên lên/xuống
tại mã 0x01/0x02. Renderer giữ nguyên đủ tám cột và tám hàng, cả dấu gạch dưới và
đuôi chữ ở hàng pixel cuối. Header `Settings` căn giữa, không có dấu `_` và không thể chọn.

- Xoay ở màn chính: chọn bốn dòng 3..6 hoặc Info ở dòng 8, theo vòng; bỏ qua dòng bị khóa và hàng trống.
- Nhấn SW: mở ngay trang con của dòng đang trỏ; không có dòng Next.
- Trong trang con: xoay di chuyển `>`; dấu `*` bên phải đánh dấu lựa chọn đã chốt.
- Nhấn SW trong trang con: chốt vào RAM và về đúng dòng ở màn chính.
- Giữ SW 750 ms: hủy nháp, quay lại màn chính; giữ ngay tại màn chính không đổi trạng thái.
- OLED: xoay xem trước contrast thật; nhấn chốt, giữ hủy và khôi phục contrast trước đó.
  Các mức OLED dừng ở hai biên; danh sách Flash/Target/Link chọn theo vòng.
- Info: nhấn SW hoặc giữ để quay lại đúng dòng Info; xoay không thay đổi dữ liệu.

SDCARD khóa **Target và Link**: hiện `--`, bỏ mũi tên mở trang và không cho encoder chọn.
Flash, OLED % và Info luôn còn truy cập. Có thể chỉnh/chốt/hủy contrast khi ở SDCARD;
contrast là cấu hình chung của cả hai chế độ. Target/Link được giữ trong RAM và xuất
hiện lại khi đổi về REMOTE.

Flash/Target/Link hiện là **cấu hình UI**. Chúng chưa đổi
dịch vụ GDB/SWD, chưa khởi động BLE/ESP-NOW/UART/I2C/SPI hay chạy job nạp độc lập từ SD.
Riêng OLED % đã nối với `probe_oled_contrast` trong task sở hữu màn hình. Chưa lưu NVS;
khởi động lại về REMOTE / SWD / WiFi / 50 %. Console và dịch vụ debug vẫn hoạt động.

Test host xác nhận buffer 8×17 byte, khoảng trống tiêu đề, năm mục điều hướng, Info chỉ đọc,
mở/đóng từng trang con, giữ Target/Link qua SDCARD và xem trước/chốt/hủy OLED trong cả hai chế độ.
Các biến thể SH1106 offset 0/2/4 và OLED tắt đều qua. Chiều xoay, số nấc và cảm nhận
thao tác encoder trên PCB vẫn cần người dùng kiểm tra trực tiếp.

Bản font 8×8 mới cũng đã qua build/test và nạp COM9, xác minh hash flash; hai lần
`status` sau nạp đều báo `oled=ONLINE`, `errors=0`. Đã đối chiếu đủ 1.024 byte với bảng
chủ dự án gửi; ảnh menu do driver thật xuất ra khớp ảnh xem trước. Bảng font nằm trong
`.rodata` với kích thước 0x400 byte. Test có kiểm tra mũi tên, đuôi chữ, dấu gạch dưới
ở hàng pixel cuối và bỏ qua mã ngoài bảng 128 glyph.

### Startup MAGIC DEBUGGER TOOL

Tên mạch chia ba dòng, mỗi từ một dòng; dùng cùng font 8×8 với menu, phóng 2 lần thành
ô 16×16 pixel. Các dòng bắt đầu tại y=5/25/45, căn giữa lần lượt trong khoảng rộng
80/128/64 pixel; dòng DEBUGGER vừa đúng tám ô lớn theo chiều ngang. Tọa độ dùng cùng
hướng đọc với menu và cấu hình xoay 180° hiện tại.

Trình tự chỉ chạy một lần khi task UI khởi động:

1. Tắt panel và giữ ảnh đen khoảng 250 ms sau khi truyền frame.
2. Quét sáng từ trái trên đến phải dưới trong 16 frame, tăng contrast đến 128.
3. Giữ toàn màn sáng thêm 150 ms.
4. Quét tắt nền cùng chiều trong 16 frame, luôn giữ các pixel thuộc ba dòng tên.
5. Giữ tên 1,2 giây, rồi chuyển thẳng sang Settings, con trỏ tại dòng 2.

Mỗi frame nhắm chu kỳ 120 ms, kể cả thời gian truyền I2C; thời gian thực tế còn phụ
thuộc bus. Do SH1106 đơn sắc, biên quét dùng ordered dithering thay cho grayscale
riêng từng pixel. Chữ cuối cùng là pixel sáng cố định trên nền đen.

Trong startup, task UI nhận và bỏ sự kiện encoder để tránh phát lại thao tác vào menu.
Wi-Fi, console, SD và SWD owner vẫn chạy; lỗi OLED kết thúc animation và đánh dấu
màn offline, không dừng probe. Các frame mô phỏng lấy trực tiếp từ test mã driver:

```powershell
.\01_source\.tooling\python-env\Scripts\python.exe .\01_source\tools\test_oled_host.py --preview-dir .\01_source\.cache\startup-20261125\frames
```

Đã kiểm tra ngày 25/11/2026: các biến thể host offset 0/2/4 và OLED tắt đều qua; kiểm
tra pixel xác nhận hai pha đơn điệu, hướng quét, ảnh toàn trắng và mặt nạ tên cuối cùng.
Đã build và nạp qua COM9, hash flash được xác minh. Một lượt reset có monitor ghi
`I (6548) probe_ui: Startup complete: MAGIC DEBUGGER TOOL`; console báo OLED online,
SD mounted, `errors=0`. Điều này xác nhận luồng firmware hoàn tất; cảm nhận hiệu ứng
và hướng nhìn trên kính thật vẫn do người dùng quan sát.

### Điều chỉnh thông số của STM32 ở giai đoạn sau

Yêu cầu "điều chỉnh mạch" được chia thành cấu hình probe ở MVP và khả năng điều chỉnh
tham số target khi có nhu cầu cụ thể ở P6, ví dụ setpoint hoặc hệ số điều khiển.
Đối với tham số target, cần danh sách biến đã cấu hình: tên, địa chỉ/symbol, kiểu dữ liệu,
đơn vị, khoảng giá trị, bước thay đổi và quyền đọc/ghi. Encoder gửi yêu cầu qua dịch vụ
probe, không tự ghi vào địa chỉ RAM bất kỳ. Hành vi áp dụng giá trị và quyền sở hữu giữa
UI với host được chốt trước khi triển khai chức năng này.

## 5. Triển khai firmware hiện tại

- `probe_board`: pin map Kconfig và kiểm tra pin trùng/chân dành riêng của profile N16R8.
- `probe_local_ui`: trạng thái menu; `probe_oled`: driver SH1106 và renderer; `probe_encoder`: input/decoder.
- OLED dùng I2C master và lớp panel-I/O của ESP-IDF v6.1. Driver SH1106 trong dự án
  gửi lệnh khởi tạo, xóa đủ RAM 132×64 trước khi bật màn, rồi ghi tám page 128 byte.
  Mỗi page đặt lại địa chỉ page và cột; framebuffer UI monochrome vẫn là 1024 byte.
- Sau startup là Settings 8×16 với bốn cấu hình và Info; model độc lập SDK, lưu lựa chọn trong RAM.
- Encoder dùng task polling 1 ms và decoder C độc lập đã có bài thử host, chưa dùng PCNT.
- UI chạy task ưu tiên thấp ở core 0; task sở hữu SWD vẫn ở core 1.
- ISR chỉ ghi nhận sự kiện hoặc thông báo task; không vẽ OLED, ghi NVS hoặc gọi SWD trong ISR.
- Các lựa chọn mode không gửi command đến target; UI chỉ áp dụng contrast thật lên OLED.
  Một dịch vụ duy nhất vẫn sở hữu SWD.
- Chỉ vẽ lại OLED khi menu thay đổi, giới hạn tối đa 5 lần/giây;
  đo và điều chỉnh sau, không giữ khóa chung với thao tác SWD trong thời gian truyền I2C.
- Wi-Fi lưu NVS khi áp dụng ở trạng thái rảnh. Clock thật qua console hiện chỉ lưu RAM.
- Lỗi hoặc thiếu OLED được báo qua log, timeout/retry có giới hạn; debug vẫn có thể tiếp tục.
- Khi host đang nạp flash hoặc sở hữu phiên debug, UI chỉ thực hiện các thao tác được dịch vụ cho phép.

### Đấu STM32 cho bài thử đầu tiên

| ESP32-S3 | STM32 |
|---|---|
| GPIO4 qua J4.2 | SWDIO |
| GPIO5 qua J4.3 | SWCLK |
| GPIO7 qua J4.5 | nRST, nếu dùng |
| GND qua J4.1 | GND |

PCB dùng tín hiệu 3,3 V, có điện trở nối tiếp R17–R20 và diode bảo vệ D5–D8 ở nhóm debug;
chưa có VTref/chuyển mức hoặc đo điện áp target trong firmware.
Target cấp nguồn riêng phù hợp board. J4.6/J4.7 là đầu vào nguồn 3,3 V/5 V của probe theo
schematic, không phải đường đo VTref trong firmware hiện tại.
GPIO6 qua J4.4 mới được dành cho SWO; chưa có thu trace. Sau đấu nối, `monitor swd_scan` mới có ý nghĩa
để nhận diện target; chưa công bố hỗ trợ flash một model chỉ từ việc build được driver.

## 6. Kế hoạch và nghiệm thu

Kiểm tra driver trên PC, không cần màn hình:

```powershell
.\01_source\.tooling\python-env\Scripts\python.exe .\01_source\tools\test_oled_host.py
.\01_source\tools\dev.ps1 build
```

Bài thử dùng mã driver thật với mô hình RAM SH1106, kiểm tra offset 0/2/4, đủ tám page,
xóa RAM trước khi bật, fallback địa chỉ, contrast, timeout và cleanup khi khởi tạo lỗi.
Mô hình này không xác nhận tín hiệu I2C hoặc chiều/căn lề trên kính thật. Sau khi nạp,
cần kiểm tra đủ tám dòng, mép trái/phải và thao tác encoder trên module đã lắp; ghi
nhận clock yêu cầu khi rảnh/bận như menu hiện có.

Ngày 25/11/2026: các biến thể host ở trên, kiểm tra hướng xoay 180° và build ESP-IDF
Debug đã qua; mã OLED được biên dịch với GNU17, `-Werror` và GCC analyzer. Bản xoay đã
nạp qua USB COM9, esptool xác minh hash thành công. Sau reset, console báo
`OLED online; encoder input task active`, `oled=ONLINE` và `errors=0`.
Chưa nghiệm thu trực quan chiều/căn lề và từng thao tác encoder trên màn thật.
Firmware nằm tại `01_source/build-vscode/stm32_wifi_probe.bin`; log cục bộ tại
`01_source/.cache/sh1106-20261125/` gồm `flash-com9-rotated.log` và `console-rotated.log`.

1. P0: đối chiếu pin map, xác nhận module encoder, nguồn/pull-up và hành vi menu.
2. P1a: phát hiện OLED, hiển thị thông tin board, đọc encoder/nút và chạy thử menu.
3. P3/P4: đưa trạng thái Wi-Fi, phiên GDB, target và lỗi thật lên màn hình.
4. P5: kiểm tra xoay/nhấn và truyền OLED trong lúc debug; đo ảnh hưởng lên SWD và mạng.
5. P6: đặc tả và đánh giá điều chỉnh tham số STM32 nếu cần.

Tiêu chí P1a: OLED hiển thị đúng, mỗi nấc menu được đếm theo cấu hình, chiều xoay đúng,
nhấn/nhấn giữ được phân biệt, không lặp nhấn do dội; tháo hoặc lỗi OLED không làm treo probe.
Thử cấp nguồn lại khi encoder/nút ở các trạng thái khác nhau.
Sau khi có debug, việc thao tác UI không gây lệnh target ngoài dự kiến hoặc làm mất phiên.

## 7. Tài liệu tham chiếu

- [Schematic PCB hiện tại, revision C](../03_pcb/layout_manual/schematic.kicad_sch).
- [Pin mặc định MCUViewer-probe](https://github.com/klonyyy/MCUViewer-probe#hardware).
- [Datasheet ESP32-S3-WROOM-1/1U](https://documentation.espressif.com/esp32-s3-wroom-1_wroom-1u_datasheet_en.pdf).
- [Pin header DevKitC-1 tham chiếu](https://documentation.espressif.com/esp-dev-kits/en/latest/esp32s3/esp32-s3-devkitc-1/user_guide_v1.0.html).
- [Datasheet SH1106 V2.3, command table và power sequence](https://files.waveshare.com/upload/0/08/SH1106_V2.3.pdf).
- [Driver SH1106 của Adafruit, tham chiếu offset mặc định 2](https://github.com/adafruit/Adafruit_SH110x/blob/master/Adafruit_SH1106G.cpp).
- [Ví dụ encoder/PCNT trong ESP-IDF v6.1](https://github.com/espressif/esp-idf/tree/v6.1/examples/peripherals/pcnt/rotary_encoder).
- [EC11 của Alps Alpine: ví dụ thông số, tín hiệu và khác biệt tại vị trí nấc](https://tech.alpsalpine.com/e/products/detail/EC11E09244AQ/).
  Chỉ là tài liệu tham khảo cùng dạng linh kiện; không xác định encoder trong ảnh là model này.
