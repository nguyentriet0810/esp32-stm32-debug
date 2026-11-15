# Linh kiện probe trên ThegioiIC — 15/11/2026

Đối chiếu từ PCB `AI_layout.kicad_pcb` hiện tại. Không thay đổi PCB, không thêm giỏ hàng hoặc đặt mua.
**Phạm vi đã cập nhật theo yêu cầu:** không tìm/mua SSD1306, rotary encoder hoặc các header. Các điện trở, tụ và linh kiện bảo vệ liên quan vẫn được giữ trong danh sách.
Số lượng cho **một PCB**. Cột **SD thêm** là phần tùy chọn thẻ SD đang DNP; **DNP khác** giữ không lắp theo bản hiện tại. Giá là giá một con/thanh tại thời điểm tra cứu; phải xét MOQ và bội số khi mua.

## Kết quả cần chú ý

- Tìm đúng module ESP32-S3-WROOM-1-N16R8, sáu nhóm bán dẫn và mười nhóm thụ động có hàng.
- **C1 4,7 µF/1206 và C2 tantalum 22 µF–16 V case B/1411 tìm đúng sản phẩm nhưng tồn kho bằng 0.**
- **BAT54S và điện trở 5,1 kΩ 1%/1206 chưa tìm được đúng loại** qua tìm kiếm trực tiếp của cửa hàng. BAT54C không thay trực tiếp cho BAT54S; điện trở 5% không được tự đánh dấu đúng BOM 1%.
- USB-C, BOOT/RESET, microSD và SMA bên dưới chỉ là ứng viên cần kiểm tra cơ khí. Không khẳng định dùng được trên PCB đã đặt chỉ từ tên gọi hoặc số chân.
- ANTEN đang DNP và chân trung tâm chưa nối RF, nên không cần mua cho chức năng antenna ngoài của phiên bản này.

## Module và IC / diode / transistor

| Cần dùng / vị trí | Cơ bản | SD thêm | DNP khác | Link sản phẩm / mã cửa hàng | Giá/đơn vị | Tồn | MOQ / bội số | Kết quả |
|---|---:|---:|---:|---|---:|---:|---|---|
| ESP32-S3-WROOM-1-N16R8 — ESP32-S3 | 1 | 0 | 0 | [ESP32-S3-WROOM-1-N16R8 Module WiFi Bluetooth 16MB Flash, 8MB PSRAM 2.4GHz](https://www.thegioiic.com/esp32-s3-wroom-1-n16r8-module-wifi-bluetooth-16mb-flash-8mb-psram-2-4ghz) · IC44278 | 172.000 đ | 32 | 1 / 1 | Có hàng; khớp tiêu chí đã đối chiếu |
| AMS1117-3.3 / SOT-223-3, tab pin 2 — AMS1117 | 1 | 0 | 0 | [AMS1117-3.3V IC Ổn Áp 3.3V 1A SOT-223](https://www.thegioiic.com/ams1117-3-3v-ic-on-ap-3-3v-1a-sot-223) · IC1025 | 3.990 đ | 2.430 | 1 / 1 | Có hàng; khớp tiêu chí đã đối chiếu |
| MMBT3904 / SOT-23 — Q1, Q2 | 2 | 0 | 0 | [MMBT3904-1AM Transistor NPN 40V 0.2A 3 Chân SOT-23](https://www.thegioiic.com/mmbt3904-1am-transistor-npn-40v-0-2a-3-chan-sot-23) · IC1337 | 399 đ | 9.502 | 20 / 10 | Có hàng; khớp tiêu chí đã đối chiếu |
| AO3401A / SOT-23 — Q3 | 1 | 0 | 0 | [AO3401A MOSFET Kênh P 30V 4A SOT23](https://www.thegioiic.com/ao3401a-mosfet-kenh-p-30v-4a-sot23) · IC11258 | 2.772 đ | 3.344 | 1 / 1 | Có hàng; khớp tiêu chí đã đối chiếu |
| BAT54S / SOT-23, dual SERIES Schottky — D5, D6, D7, D8 | 4 | 0 | 0 | Chưa tìm đúng | — | — | — / — | Chưa tìm đúng mã/thông số |
| SS34 / DO-214AC / SMA — D1, D13 | 2 | 0 | 0 | [SS34 SMA Diode Schottky 3A](https://www.thegioiic.com/ss34-sma-diode-schottky-3a) · IC2529 | 570 đ | 18.128 | 10 / 5 | Có hàng; khớp tiêu chí đã đối chiếu |
| SMAJ5.0A / DO-214AC / SMA, unidirectional TVS — D12, D14 | 2 | 0 | 0 | [SMAJ5.0A](https://www.thegioiic.com/smaj5-0a) · IC1316 | 570 đ | 7.438 | 10 / 5 | Có hàng; khớp tiêu chí đã đối chiếu |
| USBLC6-2SC6 / SOT-23-6L — D3 | 1 | 0 | 0 | [USBLC6-2SC6 TVS DIODE 5.25V 17V SOT-23-6L](https://www.thegioiic.com/usblc6-2sc6-tvs-diode-5-25v-17v-sot-23-6l) · IC45239 | 5.040 đ | 489 | 1 / 1 | Có hàng; khớp tiêu chí đã đối chiếu |

SS34 có lựa chọn [TWGMC SS34 SMA](https://www.thegioiic.com/twgmc-ss34-sma-diode-schottky-3a) giá 1.140 đ/con, MOQ1: mua đúng hai con có thể rẻ hơn tổng lô SS34 OEM MOQ10. MMBT3904 MOQ20; SMAJ5.0A MOQ10. Đây là số lượng mua, khác số lượng linh kiện cần lắp.

## Điện trở và tụ

Điện trở và tụ gốm dùng vỏ **1206 (3216 metric)**. Riêng C2 là **tantalum case B/1411, EIA-3528-21**, không phải tụ gốm 1206. Tên giá trị/vỏ đã được đối chiếu; điện môi và dung sai của một số tụ gốm chưa được cửa hàng công bố trong bảng thông số.

| Cần dùng / vị trí | Cơ bản | SD thêm | DNP khác | Link sản phẩm / mã cửa hàng | Giá/đơn vị | Tồn | MOQ / bội số | Kết quả |
|---|---:|---:|---:|---|---:|---:|---|---|
| 0 Ohm / 1206 (3216 metric) — R33 | 0 | 1 | 0 | [Điện Trở 0 Ohm 1206 1%](https://www.thegioiic.com/dien-tro-0-ohm-1206-1-) · IC6174 | 114 đ | 5.250 | 25 / 25 | Có hàng; khớp tiêu chí đã đối chiếu |
| 22 Ohm / 1206 (3216 metric) — R5, R6, R34 | 2 | 1 | 0 | [Điện Trở 22 Ohm 1206 1%](https://www.thegioiic.com/dien-tro-22-ohm-1206-1-) · IC6126 | 114 đ | 2.775 | 25 / 25 | Có hàng; khớp tiêu chí đã đối chiếu |
| 330 Ohm / 1206 (3216 metric) — R17, R18, R19, R20 | 4 | 0 | 0 | [Điện Trở 330 Ohm 1206 5%](https://www.thegioiic.com/dien-tro-330-ohm-1206-5-) · IC1635 | 87 đ | 2.750 | 25 / 25 | Có hàng; khớp tiêu chí đã đối chiếu |
| 1 KOhm / 1206 (3216 metric) — R26 | 1 | 0 | 0 | [Điện Trở 1 KOhm 1206 1%](https://www.thegioiic.com/dien-tro-1-kohm-1206-1-) · IC6142 | 176 đ | 120 | 25 / 25 | Có hàng; khớp tiêu chí đã đối chiếu |
| 4.7 KOhm / 1206 (3216 metric) — R14, R15 | 2 | 0 | 0 | [Điện Trở 4.7 KOhm 1206 1%](https://www.thegioiic.com/dien-tro-4-7-kohm-1206-1-) · IC6151 | 176 đ | 2.029 | 25 / 25 | Có hàng; khớp tiêu chí đã đối chiếu |
| 5.1 KOhm 1% / 1206 (3216 metric) — R1, R2 | 2 | 0 | 0 | Chưa tìm đúng | — | — | — / — | Chưa tìm đúng mã/thông số |
| 10 KOhm / 1206 (3216 metric) — R10, R11, R12, R13, R23, R24, R25, R36, R37, R38, R39, R40, R41 | 7 | 6 | 0 | [Điện Trở 10 KOhm 1206 5%](https://www.thegioiic.com/dien-tro-10-kohm-1206-5-) · IC1640 | 101 đ | 1.600 | 25 / 25 | Có hàng; khớp tiêu chí đã đối chiếu |
| 4.7 uF / 1206 (3216 metric) — C1 | 1 | 0 | 0 | [Tụ Gốm 1206 4.7uF 50V](https://www.thegioiic.com/tu-gom-1206-4-7uf-50v) · IC12744 | 2.772 đ | 0 | 1 / 1 | Đúng loại, hết hàng |
| 22 uF / 16V Ta / EIA-3528-21, Kemet B, approximately 3.5 x 2.8 x 2.1 mm; imperial 1411 — C2 | 1 | 0 | 0 | [Tụ Tantalum 22uF 16V 1411 T491B226K016AT](https://www.thegioiic.com/tu-tantalum-22uf-16v-1411-t491b226k016at) · IC28120 | 17.100 đ | 0 | 1 / 1 | Đúng loại, hết hàng |
| 10 uF / 1206 (3216 metric) — C3, C15 | 1 | 1 | 0 | [Tụ Gốm 1206 10uF 16V](https://www.thegioiic.com/tu-gom-1206-10uf-16v) · IC2794 | 1.140 đ | 1.675 | 1 / 1 | Có hàng; khớp tiêu chí đã đối chiếu |
| 100 nF / 1206 (3216 metric) — C4, C10, C16 | 2 | 1 | 0 | [Tụ Gốm 1206 100nF (0.1uF) 50V](https://www.thegioiic.com/tu-gom-1206-100nf-0-1uf-50v) · IC7222 | 570 đ | 5.094 | 10 / 5 | Có hàng; khớp tiêu chí đã đối chiếu |
| 1 uF / 1206 (3216 metric) — C9 | 1 | 0 | 0 | [Tụ Gốm 1206 1uF 50V](https://www.thegioiic.com/tu-gom-1206-1uf-50v) · IC7224 | 735 đ | 3.970 | 10 / 5 | Có hàng; khớp tiêu chí đã đối chiếu |
| 10 pF / 1206 (3216 metric) — C7, C8 | 0 | 0 | 2 | [Tụ Gốm 1206 10pF 50V](https://www.thegioiic.com/tu-gom-1206-10pf-50v) · IC7221 | 456 đ | 609 | 20 / 10 | Có hàng; khớp tiêu chí đã đối chiếu |

## USB-C, nút nhấn và socket tùy chọn

| Cần dùng / vị trí | Cơ bản | SD thêm | DNP khác | Link sản phẩm / mã cửa hàng | Giá/đơn vị | Tồn | MOQ / bội số | Kết quả |
|---|---:|---:|---:|---|---:|---:|---|---|
| G-Switch GT-USB-7010ASV — TYPE-C | 1 | 0 | 0 | [Cổng USB Cái 3.1 Type C 16 Chân Thẳng Dán SMD](https://www.thegioiic.com/cong-usb-cai-3-1-type-c-16-chan-thang-dan-smd) · IC36686 | 1.710 đ | 17 | 1 / 1 | Chỉ là ứng viên; chưa chốt lắp vừa |
| C&K PTS636Sx25SMTRLFS footprint family — BOOT, RESET | 2 | 0 | 0 | [Nút Nhấn 3x6mm Cao 2.5mm 2 Chân SMD Đầu Trắng](https://www.thegioiic.com/nut-nhan-3x6mm-cao-2-5mm-2-chan-smd-dau-trang) · IC3592 | 570 đ | 2.559 | 10 / 5 | Chỉ là ứng viên; chưa chốt lắp vừa |
| JXTCONN TF-01A — SD SOCKET | 0 | 1 | 0 | [MiSD-PP-9P-V1 Khay Thẻ Nhớ MicroSD 9 Chân Push-Push](https://www.thegioiic.com/misd-pp-9p-v1-khay-the-nho-microsd-9-chan-push-push) · IC4602 | 2.508 đ | 92 | 1 / 1 | Chỉ là ứng viên; chưa chốt lắp vừa |
| BAT Wireless BWSMA-KWE-Z001 — ANTEN | 0 | 0 | 1 | [SMA-KWE-14.5 Đầu RF SMA Đực Dài 14.5mm Vuông Góc 90 Độ Hàn PCB](https://www.thegioiic.com/sma-kwe-14-5-dau-rf-sma-duc-dai-14-5mm-vuong-goc-90-do-han-pcb) · IC5095 | 11.400 đ | 325 | 1 / 1 | Chỉ là ứng viên; chưa chốt lắp vừa |

## Các điểm tránh mua nhầm

- ESP32 phải là module rời **WROOM-1-N16R8** có antenna PCB, không phải DevKit/CAM board hoặc WROOM-1U.
- SS34 phải **SMA**, không chọn SMB; SMAJ5.0**A** một chiều, không tự thay bằng **CA** hai chiều.
- USBLC6-2**SC6** là SOT-23-6; bản **P6** dùng package khác.
- BOOT/RESET của PCB là switch SMD **hai pad** tâm cách 7,75 mm. Nút OEM 3×6 mm có bản vẽ gần tương tự nhưng chưa được xác nhận thay trực tiếp.
- MicroSD IC4602 không được xác nhận là JXTCONN TF-01A; cần khớp toàn bộ chân vỏ, detect và land pattern.
- C7/C8 10 pF đang DNP và không thuộc cụm SD. Cụm SD cần socket + R33/R34/R36–R41 + C15/C16 khi lắp tùy chọn.

## Nguồn và bằng chứng

- [BOM gốc trích từ PCB](pcb_bom.json)
- [Bán dẫn và đối chiếu pinout](semiconductors.md) · [dữ liệu giá/tồn](semiconductors.json)
- [Điện trở/tụ](passives.md) · [dữ liệu trực tiếp](passives.json)
- [Đầu nối và bản vẽ cơ khí](connectors.md) · [dữ liệu trực tiếp](connectors.json)
- [Dữ liệu module đã đọc trực tiếp trên trang; OLED thuộc phần đã loại khỏi danh sách mua](modules.json)
- [Danh sách chuẩn hóa 25 nhóm trong phạm vi hiện tại](shopping_list.json)
- [Phạm vi loại trừ theo yêu cầu](scope.json)

Giá/tồn kho là ảnh chụp dữ liệu ngày 15/11/2026, có thể thay đổi khi bạn đặt mua. Chưa tính phí giao hàng. Không có tổng giá hoàn thiện board vì một số linh kiện thiếu/hết hàng hoặc chưa chốt cơ khí.
