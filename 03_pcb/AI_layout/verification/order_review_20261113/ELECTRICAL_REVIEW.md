# Kiểm tra điện và linh kiện trước đặt PCB - 13/11/2026

Phạm vi: đọc PCB và schematic, kiểm tra topology, pinout và bản vẽ nhà sản xuất; không thay đổi PCB/schematic. PCB được kiểm tra có SHA-256 `82da382a408244f0554dbf6b1e7cf92267938712c95e73f0253b3162a39e05fb`. Inventory bằng chứng: `electrical_inventory.json`. DRC và kiểm tra bộ Gerber do báo cáo khác thực hiện.

## Kết luận

Không tìm thấy lỗi đảo chân hay sai topology mới trong các cụm USB, auto-reset, SD và nguồn được kiểm tra. Chưa đủ bằng chứng để xác nhận sản xuất hàng loạt hoặc mọi chế độ cấp nguồn đều hoạt động ổn định. Có thể tiến tới PCB thử nghiệm sau khi chốt tương thích cơ khí đầu USB với board 1,6 mm và phương án lắp cụm SD. Những điểm còn lại là điều kiện sử dụng/kiểm chứng, không tự động là lỗi chế tạo bo trần.

## Điểm cụ thể cần chốt trước đặt

1. **USB-C và độ dày PCB:** bản vẽ đúng mã G-Switch GT-USB-7010ASV, Rev A0 ngày 2023.02.04, ghi `Thickness 0.80MM` ngay dưới `Recommended P.C.B.Layout`. PCB đang là 1,6 mm theo yêu cầu giữ nguyên. Đây là đầu SMT đặt trên mặt board với bốn chân vỏ vào rãnh mạ; không phải đầu mid-mount. Chưa chứng minh được 1,6 mm gây cấn, nhưng bản vẽ không xác nhận chất lượng lắp/hàn và giữ cơ khí cho độ dày này. Cần nhà cung cấp xác nhận hoặc thử lắp linh kiện thực trước khi chốt. Không diễn giải ghi chú này thành độ dày vỏ kim loại: vỏ có kích thước riêng 0,30 mm. Xem bản vẽ đã lưu `sources/GT-USB-7010ASV.pdf` và ảnh toàn trang.
2. **Variant SD:** socket, R33/R34/R36-R41 và C15/C16 vẫn DNP như nguồn gốc. Nếu đơn lắp ráp tự bỏ toàn bộ DNP thì SD không hoạt động. R33 là cầu cấp nguồn SD; R34 nằm nối tiếp CLK. Phải ghi rõ lắp cụm SD trên BOM khi cần dùng chức năng này; không cần sửa đồng chỉ để đổi phương án lắp.

## Kết quả kiểm tra từng cụm

| Cụm | Bằng chứng và đánh giá |
|---|---|
| Auto-reset | Q1: pin1 B nhận DTR qua R10, pin2 E = RTS, pin3 C = EN. Q2: pin1 B nhận RTS qua R11, pin2 E = DTR, pin3 C = BOOT/GPIO0. Đúng mạch chéo hai NPN; footprint MMBT3904 B1/E2/C3 đúng. EN có R12 10 kΩ kéo lên và C9 1 µF xuống GND; BOOT có R13 10 kΩ. Tổ hợp điện áp DTR/RTS H/H và L/L đều nhả reset; H/L kéo EN thấp; L/H kéo BOOT thấp. Chưa đo dạng sóng trên bo thật. |
| USB-C/ESD | A6/B6 = D+, A7/B7 = D-, các pad GND/VBUS trùng tọa độ là các tiếp điểm chung có chủ ý. CC1/CC2 có mỗi điện trở 5,1 kΩ riêng xuống GND. D3 USBLC6-2SC6 đúng pinout: 1/6 D-, 3/4 D+, 2 GND, 5 VBUS_OR. Vỏ USB nối GND trực tiếp; không phát hiện sai chân từ lựa chọn này. |
| USB tín hiệu | GPIO19/20 đúng D-/D+. R5/R6 22 Ω, C7/C8 10 pF DNP là reservation hợp lý. Cân chiều dài không chứng minh 90 Ω; cần nhà máy xác nhận hình học/stackup và kiểm tra USB thực. Chưa có solver/TDR hoặc thử eye/ESD hệ thống. |
| ESP32-S3 N16R8 | SPI SD dùng GPIO38/39/40/41, detect GPIO42; không dùng GPIO35-37 dành cho Octal PSRAM. USB và GPIO0/EN đúng. Antenna của WROOM-1 nằm ngoài mép trên board; rule area cấm track/via/zone phủ vùng này. Không phát hiện đồng dưới antenna trong vùng board. Footprint đã tùy chỉnh vẫn cần chấp nhận theo linh kiện thực và quy trình hàn exposed pad. |
| SMA ANTEN | Pad trung tâm hiện NC, bốn pad vỏ GND; có cờ DNP. WROOM-1 dùng antenna PCB tích hợp, không có đường RF từ module tới SMA này. Không thể dùng đầu SMA làm antenna ngoài trong cấu hình hiện tại. Đây là giới hạn chức năng có sẵn, không phải lỗi sản xuất bo trần. |
| microSD | Bản vẽ TF-01A xác nhận chân 1 DAT2, 2 DAT3/CS, 3 CMD/MOSI, 4 VDD, 5 CLK, 6 VSS, 7 DAT0/MISO, 8 DAT1 và tiếp điểm CD riêng đóng xuống shell GND khi cắm thẻ. PCB gán chân tương ứng đúng. Listing phân phối có mục 'No card detection' nhưng trái với bản vẽ có switch; dùng bản vẽ làm bằng chứng pinout. |
| Nguồn 5 V | D1/D13 SS34 có anode ở từng đầu vào 5 V và cathode tại VBUS_OR: đúng hướng diode OR. AMS1117 pin1 GND, pin2 và tab là 3V3, pin3 VIN đúng. C2 22 µF tantalum phù hợp hướng dẫn ổn định AMS. Không phát hiện đảo chân ổn áp. |

## Giới hạn nguồn cần kiểm chứng trên prototype

- AMS1117 sau SS34 làm giảm headroom. Datasheet AMS quy định dropout tối đa 1,3 V tại 0,8 A; để bảo đảm 3,3 V ở dòng đó cần ít nhất 4,6 V ngay tại chân VIN, còn phải cộng sụt SS34 và dây dẫn ở phía nguồn. Điều này **không chứng minh bo sẽ lỗi ở tải thực thấp hơn**, nhưng chưa thể bảo đảm toàn dải cáp/nguồn và tải SD/Wi-Fi. ESP32-S3 yêu cầu nguồn có khả năng cung cấp ít nhất 0,5 A; cần đo rail khi khởi động và khi Wi-Fi + SD cùng hoạt động, đồng thời đo nhiệt AMS1117.
- Q3 AO3401A: G = GND, S = 3V3 board, D = J4_3V3_IN. Đây là bảo vệ ngược cực theo schematic, **không phải cách ly nguồn hai chiều**. Khi 3V3 board có điện, VGS âm làm Q3 dẫn nên có thể cấp ngược ra chân J4.6. Không đấu cạnh tranh hai nguồn 3,3 V; các chân J4.6/7 được tài liệu dự án coi là đầu vào nguồn, không phải chân đo VTref của target.
- SMAJ5.0A trên đầu vào 3,3 V không biến đầu đó thành đầu vào chịu 5 V; phải dùng 3,30 V đã ổn áp theo ghi chú schematic.

## Nguồn kiểm chứng

- [Bản vẽ nhà sản xuất G-Switch GT-USB-7010ASV](https://datasheet.lcsc.com/datasheet/pdf/a1d5a66cf718dd88763ab794dba802b2.pdf?productCode=C2988369), toàn trang 1; đã lưu PDF và PNG để đối chiếu.
- [Bản vẽ nhà sản xuất JXTCONN TF-01A](https://datasheet.lcsc.com/datasheet/pdf/e91427bcf127fa7b43a5344e03fa8f57.pdf?productCode=C41426929), trang bản vẽ 1/2; đã lưu PDF và PNG.
- [AMS1117, Advanced Monolithic Systems](https://datasheet.lcsc.com/datasheet/pdf/e6935943fc6b1bbf350a1a0f3e90dc4a.pdf?productCode=C6186), pinout, dropout/stability/thermal.
- [AO3401A, AOS](https://www.aosmd.com/sites/default/files/res/data_sheets/AO3401A.pdf), pinout và đặc tính P-MOS.
- [MMBT3904, onsemi](https://www.onsemi.com/pdf/datasheet/mmbt3904lt1-d.pdf), B1/E2/C3.
- [USBLC6-2, ST](https://www.st.com/resource/en/datasheet/usblc6-2.pdf), pinout và hướng dẫn kết nối ESD ngắn.
- [ESP32-S3-WROOM-1/-1U, Espressif](https://www.espressif.com/sites/default/files/documentation/esp32-s3-wroom-1_wroom-1u_datasheet_en.pdf), pin mapping, variant antenna và nguồn.
- [Espressif auto-reset](https://docs.espressif.com/projects/esptool/en/latest/esp32s3/advanced-topics/boot-mode-selection.html) và [hướng dẫn PCB](https://docs.espressif.com/projects/esp-hardware-design-guidelines/en/latest/esp32s3/pcb-layout-design.html).
