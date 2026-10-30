# Phần cứng và giao diện điều khiển tại probe

Trạng thái: **đã có driver/menu; đã chọn encoder rời theo ảnh, chưa nghiệm thu ngoại vi/target thật**.

## 1. Yêu cầu đã xác nhận

- Probe: ESP32-S3 N16R8 dạng DevKit có chân đưa ra sẵn, ESP-IDF v6.1.
- Giữ nhóm chân debug của dự án MCUViewer-probe tham khảo.
- Thêm OLED 0,96 inch, I2C, **SSD1306 128×64** theo xác nhận của người dùng.
- Thêm một rotary encoder để thao tác menu và điều chỉnh thông số khi cần.
- Ngày 30/10/2026, người dùng chọn encoder cơ khí rời có nút nhấn theo ảnh đã cung cấp.
  Hình dáng giống dòng EC11, với nhóm A/B/common và cặp tiếp điểm nút nhấn riêng.
  Ảnh chưa cho thấy mã linh kiện nên chưa xác nhận hãng, model, pinout hoặc số xung/nấc.

## 2. Pin map mặc định trong firmware

Các số dưới đây là **số GPIO**, không phải số thứ tự chân trên header DevKit.
Đối chiếu nhãn GPIO và mạch nối của board thực tế trước khi chốt đấu dây.

| Nhóm | Tín hiệu | ESP32-S3 | Ghi chú |
|---|---|---|---|
| Debug | SWDIO | GPIO4 | Giữ mặc định dự án mẫu; hai chiều |
| Debug | SWCLK | GPIO5 | Giữ mặc định dự án mẫu; output |
| Debug | SWO | GPIO6 | Giữ mặc định dự án mẫu; input, dùng ở giai đoạn trace |
| Debug | Target nRST | GPIO10 | Giữ mặc định dự án mẫu; open-drain, không phải EN/reset của S3 |
| OLED | SDA | GPIO8 | Bus I2C riêng cho giao diện |
| OLED | SCL | GPIO9 | Bus I2C riêng cho giao diện |
| Encoder | A / CLK | GPIO11 | Input quadrature A |
| Encoder | B / DT | GPIO12 | Input quadrature B |
| Encoder | SW | GPIO13 | Input nút nhấn active-low, đầu còn lại của công tắc nối GND |
| Tham chiếu | Status LED | GPIO21 | Giữ dự phòng theo LED của dự án mẫu |
| Tham chiếu | BOOT/setup | GPIO0 | Giữ nút BOOT có sẵn; không dùng cho encoder |
| Nguồn | OLED VCC | 3V3 | Module phải hỗ trợ nguồn 3,3 V và mức I2C 3,3 V |
| Nguồn | GND | GND | OLED, encoder, S3 và target dùng chung mốc GND |

GPIO21 là chân LED trên board Waveshare của dự án mẫu. LED tích hợp trên DevKit S3 có
thể ở chân khác; cần đối chiếu board trước khi cấu hình LED, không suy ra từ dự án mẫu.

Pin map này giữ GPIO19/20 cho USB, GPIO43/44 cho UART0 và không dùng các chân strapping
cho linh kiện UI mới. GPIO35/36/37 của module R8 dùng cho Octal PSRAM nên không được
đưa vào nhóm mở rộng. Việc chọn GPIO8/9/11/12/13 dựa trên module và DevKit tham chiếu;
mạch phụ trên board thực tế vẫn cần được kiểm tra.

## 3. Đấu nối OLED và encoder

### OLED SSD1306

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
SDA/SCL cần pull-up về 3,3 V; kiểm tra điện trở đã có trên module trước khi bổ sung.

### Rotary encoder rời đã chọn

Đấu theo chức năng chân sau khi xác nhận pinout của linh kiện thực tế:

| Chức năng chân encoder | Nối tới | Vai trò |
|---|---|---|
| A | GPIO11 | Pha A quadrature |
| B | GPIO12 | Pha B quadrature |
| C / common | GND | Chân chung của hai tiếp điểm xoay |
| Một chân công tắc nhấn | GPIO13 | SW active-low |
| Chân công tắc nhấn còn lại | GND | Nhấn sẽ kéo GPIO13 xuống thấp |

Đây là sơ đồ theo chức năng, không phải thứ tự chân trái/phải nhìn từ ảnh. Không gán
chân giữa là C hoặc xác định A/B chỉ từ góc chụp. Các tai kim loại lớn dùng để cố định
vỏ không được dùng thay chân common khi chưa kiểm tra thông số linh kiện.

Encoder tiếp điểm rời dùng cách đấu trên không cần chân VCC riêng. A/B/SW được kéo lên
mức logic 3,3 V qua pull-up. Driver hiện đã bật pull-up nội cho GPIO11/12/13, phù hợp để
bắt đầu thử trên dây ngắn; lựa chọn pull-up ngoài/lọc RC sẽ theo thông số và phép đo của
linh kiện thực tế. Không đưa mức 5 V vào GPIO ESP32.

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

Nguồn nguyên mẫu sẽ được chốt theo DevKit và target thực tế. Không suy ra khả năng cấp
nguồn cho S3/OLED từ một chân nguồn trên header debug của target.

## 4. Nội dung hiển thị và thao tác

### Menu hiện đã triển khai

| Trang | Nội dung |
|---|---|
| Tổng quan | Wi-Fi/IP, tên target nếu có, clock SWD danh định, trạng thái GDB |
| Clock | Xoay chọn 100/400 kHz, 1/2/4/8 MHz; nhấn áp dụng khi rảnh; giữ để quay lại |

Ví dụ bố cục, các giá trị chỉ là minh họa:

```text
STM32 PROBE
WiFi: CONNECTED
IP:   192.168.1.50
MCU:  STM32F103
SWD:  400 kHz
GDB:  IDLE
```

- Xoay trong trang clock: đổi tần số được chọn.
- Nhấn ngắn: chọn mục hoặc áp dụng giá trị đang sửa.
- Nhấn giữ: quay lại/hủy giá trị đang sửa; không gán thao tác reset target vào nút giữ mặc định.
- Những thông số đang do phiên host sở hữu được hiển thị chỉ đọc hoặc báo BUSY.
- Màn hình phân biệt clock đang áp dụng với giá trị đang chọn. Đây là giá trị danh định
  trong port, chưa phải tần số đo bằng logic analyzer. Thay đổi chưa lưu NVS.

Wi-Fi/IP và trạng thái target lấy từ snapshot thật. Khi chưa có target, hiển thị NONE.
Trang thông tin, menu contrast, timeout màn hình và lưu cấu hình UI là phần còn lại của P1a.

### Điều chỉnh thông số của STM32 ở giai đoạn sau

Yêu cầu "điều chỉnh mạch" được chia thành cấu hình probe ở MVP và khả năng điều chỉnh
tham số target khi có nhu cầu cụ thể ở P6, ví dụ setpoint hoặc hệ số điều khiển.
Đối với tham số target, cần danh sách biến đã cấu hình: tên, địa chỉ/symbol, kiểu dữ liệu,
đơn vị, khoảng giá trị, bước thay đổi và quyền đọc/ghi. Encoder gửi yêu cầu qua dịch vụ
probe, không tự ghi vào địa chỉ RAM bất kỳ. Hành vi áp dụng giá trị và quyền sở hữu giữa
UI với host được chốt trước khi triển khai chức năng này.

## 5. Triển khai firmware hiện tại

- `probe_board`: pin map Kconfig và kiểm tra pin trùng/chân dành riêng của profile N16R8.
- `probe_local_ui`: trạng thái menu; `probe_oled`: renderer SSD1306; `probe_encoder`: input/decoder.
- OLED dùng I2C master driver và panel SSD1306 sẵn có trong ESP-IDF v6.1, với renderer
  chữ/menu gọn. Framebuffer monochrome 128×64 cần 1024 byte cho một bản hình.
- Encoder dùng task polling 1 ms và decoder C độc lập đã có bài thử host, chưa dùng PCNT.
- UI chạy task ưu tiên thấp ở core 0; task sở hữu SWD vẫn ở core 1.
- ISR chỉ ghi nhận sự kiện hoặc thông báo task; không vẽ OLED, ghi NVS hoặc gọi SWD trong ISR.
- UI nhận snapshot trạng thái và gửi command qua queue; một dịch vụ duy nhất sở hữu target.
- Cập nhật OLED giới hạn tối đa 5 lần/giây;
  đo và điều chỉnh sau, không giữ khóa chung với thao tác SWD trong thời gian truyền I2C.
- Wi-Fi lưu NVS khi áp dụng ở trạng thái rảnh. Clock UI/console hiện chỉ lưu RAM.
- Lỗi hoặc thiếu OLED được báo qua log, timeout/retry có giới hạn; debug vẫn có thể tiếp tục.
- Khi host đang nạp flash hoặc sở hữu phiên debug, UI chỉ thực hiện các thao tác được dịch vụ cho phép.

### Đấu STM32 cho bài thử đầu tiên

| ESP32-S3 | STM32 |
|---|---|
| GPIO4 | SWDIO |
| GPIO5 | SWCLK |
| GPIO10 | nRST, nếu dùng |
| GND | GND |

Nguyên mẫu dùng tín hiệu 3,3 V, chưa có VTref/chuyển mức/bảo vệ hoặc đo điện áp target.
Target cấp nguồn riêng phù hợp board. Chân 3V3 không phải đường đo VTref trong firmware hiện tại.
GPIO6 mới được dành cho SWO; chưa có thu trace. Sau đấu nối, `monitor swd_scan` mới có ý nghĩa
để nhận diện target; chưa công bố hỗ trợ flash một model chỉ từ việc build được driver.

## 6. Kế hoạch và nghiệm thu

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

- [Pin mặc định MCUViewer-probe](https://github.com/klonyyy/MCUViewer-probe#hardware).
- [Datasheet ESP32-S3-WROOM-1/1U](https://documentation.espressif.com/esp32-s3-wroom-1_wroom-1u_datasheet_en.pdf).
- [Pin header DevKitC-1 tham chiếu](https://documentation.espressif.com/esp-dev-kits/en/latest/esp32s3/esp32-s3-devkitc-1/user_guide_v1.0.html).
- [Ví dụ SSD1306 trong ESP-IDF v6.1](https://github.com/espressif/esp-idf/tree/v6.1/examples/peripherals/lcd/i2c_oled).
- [Ví dụ encoder/PCNT trong ESP-IDF v6.1](https://github.com/espressif/esp-idf/tree/v6.1/examples/peripherals/pcnt/rotary_encoder).
- [EC11 của Alps Alpine: ví dụ thông số, tín hiệu và khác biệt tại vị trí nấc](https://tech.alpsalpine.com/e/products/detail/EC11E09244AQ/).
  Chỉ là tài liệu tham khảo cùng dạng linh kiện; không xác định encoder trong ảnh là model này.
