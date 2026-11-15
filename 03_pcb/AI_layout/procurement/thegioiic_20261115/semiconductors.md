# Bán dẫn AI_layout trên thegioiic.com - 15/11/2026

Dữ liệu giá/tồn kho được đọc trực tiếp từ trang công khai lúc khoảng 18:10-18:15 (Asia/Bangkok), không lấy giá cache của công cụ tìm kiếm. Số lượng cho một board; chưa thêm giỏ, chưa đặt hàng, chưa sửa PCB. Giá/tồn kho có thể thay đổi.

| Vị trí | Cần | Sản phẩm đúng mã/package | Giá/con | Tồn kho | Mua tối thiểu / bội số | Kết quả |
|---|---:|---|---:|---:|---|---|
| AMS1117 | 1 | [AMS1117-3.3V IC Ổn Áp 3.3V 1A SOT-223](https://www.thegioiic.com/ams1117-3-3v-ic-on-ap-3-3v-1a-sot-223) (IC1025) | 3.990 đ | 2.430 | 1 / 1 | Khớp package và điện |
| Q1, Q2 | 2 | [MMBT3904-1AM Transistor NPN 40V 0.2A 3 Chân SOT-23](https://www.thegioiic.com/mmbt3904-1am-transistor-npn-40v-0-2a-3-chan-sot-23) (IC1337) | 399 đ | 9.502 | 20 / 10 | Khớp package và điện |
| Q3 | 1 | [AO3401A MOSFET Kênh P 30V 4A SOT23](https://www.thegioiic.com/ao3401a-mosfet-kenh-p-30v-4a-sot23) (IC11258) | 2.772 đ | 3.344 | 1 / 1 | Khớp package và điện |
| D5, D6, D7, D8 | 4 | [BAT54S SOT-23, dual series](https://www.thegioiic.com/tim-kiem?q=BAT54S) | - | - | - | Chưa tìm thấy; site trả 0 |
| D1, D13 | 2 | [SS34 SMA Diode Schottky 3A](https://www.thegioiic.com/ss34-sma-diode-schottky-3a) (IC2529) | 570 đ | 18.128 | 10 / 5 | Khớp package và điện |
| Lựa chọn SS34 khác | 2 | [TWGMC SS34 SMA Diode Schottky 3A](https://www.thegioiic.com/twgmc-ss34-sma-diode-schottky-3a) (IC36304) | 1.140 đ | 5.983 | 1 / 1 | Khớp package và điện |
| D12, D14 | 2 | [SMAJ5.0A](https://www.thegioiic.com/smaj5-0a) (IC1316) | 570 đ | 7.438 | 10 / 5 | Khớp package và điện |
| D3 | 1 | [USBLC6-2SC6 TVS DIODE 5.25V 17V SOT-23-6L](https://www.thegioiic.com/usblc6-2sc6-tvs-diode-5-25v-17v-sot-23-6l) (IC45239) | 5.040 đ | 489 | 1 / 1 | Khớp package và điện |

## Chỗ dễ mua nhầm

- **AMS1117-3.3:** 1=GND; 2/tab=VOUT; 3=VIN. Chọn phiên bản 3.3V cố định, không chọn ADJ/5V/TO-252. C2 22uF tantalum được giữ nguyên như PCB.
- **MMBT3904:** 1=B; 2=E; 3=C. Mã 1AM phù hợp datasheet Jiangsu gắn trên trang; nhà bán lẻ ghi OEM/China. Không thay bằng 2N3904 TO-92.
- **AO3401A:** 1=G; 2=S; 3=D. P-channel, đúng AO3401A; không chọn AO3400A N-channel.
- **BAT54S:** 1=anode diode1; 2=cathode diode2; 3=series midpoint. Tìm kiếm trực tiếp BAT54S trả 0; danh mục Schottky61 và arrays9 không có mã S. BAT54C/Common Cathode và BAT54A/Common Anode không tương thích, không mua thay.
- **SS34:** 1=K cathode (đầu có vạch); 2=A anode. Hai lựa chọn cùng SS34 SMA40V3A. IC2529 OEM rẻ mỗi con nhưng min10; IC36304 TWGMC min1 nên hai con rẻ hơn tổng lô OEM. Không chọn SS34 SMB IC11422 hoặc SMC.
- **SMAJ5.0A:** 1=K cathode (đầu có vạch); 2=A anode. Đúng suffix A một chiều; không mua SMAJ5.0CA hai chiều. Datasheet chỉ V_RWM=5V, không đồng nghĩa kẹp ở5V.
- **USBLC6-2SC6:** 1/6=I/O1; 3/4=I/O2; 2=GND; 5=VBUS. Mã sản phẩm trên title đúngSC6, STMicroelectronics; trường manufacturer_code nội bộ trống. ST official datasheet xác nhậnSC6 SOT23-6L. Không mua USBLC6-2P6 SOT-666.

**Tiền hàng tối thiểu cho sáu nhóm tìm được:** 31.182 đ nếu chọn SS34 OEM min10; hoặc 27.762 đ nếu mua đúng2 SS34 TWGMC. Chưa có4 BAT54S, chưa gồm linh kiện khác/phí vận chuyển. Hai tổng này đã tính MMBT3904 min20 và SMAJ5.0A min10.

## Bằng chứng

- `semiconductors.json`: dữ liệu chi tiết, MOQ, số lượng lô tối thiểu, link datasheet và thời điểm tải từng trang.
- `sources/*.html`, `*.rsc.txt`, `*.products.json`: dữ liệu công khai đã lưu.
- [ST USBLC6-2 datasheet](https://www.st.com/resource/en/datasheet/usblc6-2.pdf) xác nhận pinout và khác biệt SC6/P6.
- [Jiangsu MMBT3904 datasheet](https://file.thegioiic.com/upload/documents/1734592101_Jiangsu-Changjing-Electronics-Technology-Co---Ltd--MMBT3904.pdf) xác nhận B1/E2/C3.
- [AO3401A của AOS](https://www.aosmd.com/sites/default/files/res/data_sheets/AO3401A.pdf), [SS34 SMA](https://file.thegioiic.com/upload/documents/1740544899_SS34_SMA.pdf), [SMAJ5.0A](https://file.thegioiic.com/upload/documents/1740557455_SMAJ5.0A.pdf).
