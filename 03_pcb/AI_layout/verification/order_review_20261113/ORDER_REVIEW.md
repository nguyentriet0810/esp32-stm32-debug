# Đánh giá đặt PCB — AI_layout — 13/11/2026

**Kết luận: file PCB đạt các kiểm tra hình học gia công, kết nối và CAM đã thực hiện. Có thể tiến tới đặt lô nguyên mẫu sau khi xác nhận đầu USB-C dùng được trên board 1,6 mm và nhà máy nhận rãnh mạ 0,6 mm. Chưa đủ bằng chứng để chốt sản xuất hàng loạt hoặc cam kết mọi chức năng đã chạy trên phần cứng.**

Đánh giá chỉ đọc trên `AI_layout.kicad_pcb` đã lưu, SHA-256 `82da382a408244f0554dbf6b1e7cf92267938712c95e73f0253b3162a39e05fb`. Không thay đổi PCB, schematic, project hoặc thư viện trong lần đánh giá này.

## Thay đổi chữ của người dùng

Đối chiếu với bản trước SHA-256 `adfa08d080b39db993dff03f7276c7955daf49b9553c37edc63b007e2c0585dc`:

- Chỉ đổi vị trí/góc chữ reference R24, R37, D3, R34, D6, D1, Q1 và nhãn board `AI_layout` thành `REV 1.0`.
- Tên reference và giá trị linh kiện không đổi. Toàn bộ copper, pad/net, footprint, lỗ/rãnh, vùng đồng đã fill, stackup và hai gốc tọa độ không đổi.
- Chữ mới không tạo lỗi chồng silkscreen, bị solder mask cắt, sát mép, thiếu chiều cao hoặc thiếu độ dày nét theo rule hiện tại.

Bằng chứng: [đối chiếu và DFM](text_change_and_dfm_audit.json), [diff chính xác](pcb_text_changes.diff).

## Kết quả kiểm tra

| Hạng mục | Kết quả |
|---|---|
| DRC KiCad 10.0.6, all-track-errors, refill zones | 0 lỗi, 0 kết nối hở |
| Silkscreen sau đổi chữ | 0 cảnh báo overlap/mask/edge/text |
| Schematic đối chiếu pad theo UUID | 207 kết nối chân, 0 thiếu component, 0 sai net |
| Track nhỏ nhất / nguồn nhỏ nhất | 0,20 / 0,40 mm |
| Rule clearance đồng | 0,20 mm; DRC đạt |
| Lỗ via nhỏ nhất / annular ring via nhỏ nhất | 0,40 / 0,20 mm |
| Nét chữ / chiều cao chữ nhỏ nhất | 0,15 / 1,00 mm |
| Board | 70 × 60 mm, R3, 2 lớp, FR4 1,6 mm, đồng 35 µm/mặt |
| Phủ GND | Không thay đổi so với bản đã kiểm tra 62 pad GND liên thông, 42 via GND, 0 đảo cô lập |
| Gốc Gerber và khoan | Chung gốc dưới trái; tâm đường bao X 0…70, Y 0…60 mm |
| Khoan/rãnh so với pad đồng | Đủ 160 PTH và 8 NPTH; sai khác tối đa 0,0005 mm do làm tròn Excellon |
| CAM copper/mask vượt ngoài board | Không phát hiện tại độ phân giải raster 0,025 mm/pixel |
| CAM chữ chồng cửa mask | Không phát hiện ở hai mặt tại cùng độ phân giải |

Các kích thước trên đáp ứng giới hạn track/clearance 0,15 mm, lỗ khoan 0,30 mm, vòng đồng via 0,15 mm và nét chữ 0,15 mm trong ảnh người dùng, đồng thời khớp [thông số SmartPCB88 đang công bố](https://www.smartpcb88.com/).

Các kiểm tra CAM raster bổ trợ cho DRC hình học, không thay thế DRC. Nguồn bằng chứng: [DRC](drc.json), [đối chiếu schematic](netlist_comparison.json), [CAM](CAM_REVIEW.md), [kiểm tra gốc](origin_check/origin_validation.json), [ảnh mặt trên](CAM_top.png), [ảnh mặt dưới](CAM_bottom.png).

## Hai xác nhận trước khi chốt đơn

1. **USB-C GT-USB-7010ASV trên PCB dày 1,6 mm.** Bản vẽ G-Switch ghi `Thickness 0.80MM` ngay dưới hình recommended PCB layout; board hiện giữ 1,6 mm theo yêu cầu ban đầu. Đây là đầu SMT trên mặt board với bốn chân vỏ cắm vào rãnh, nên không đủ cơ sở khẳng định không lắp được. Cần xác nhận nhà cung cấp hoặc thử linh kiện thật để chắc chắn độ ăn chân vỏ và khả năng hàn/giữ cơ khí trên 1,6 mm. Không tự giảm độ dày board hoặc đổi footprint. [Bản vẽ hãng](https://datasheet.lcsc.com/datasheet/pdf/a1d5a66cf718dd88763ab794dba802b2.pdf?productCode=C2988369), [bản lưu](sources/GT-USB-7010ASV.pdf).
2. **Khả năng rãnh mạ của nhà máy.** PCB có bốn rãnh vỏ USB rộng 0,6 mm, dài 1,4 hoặc 1,7 mm. Cần nhà máy xác nhận gia công rãnh PTH 0,6 mm; thông số lỗ tròn tối thiểu 0,3 mm trong ảnh chưa xác nhận khả năng phay rãnh mạ. Hai rãnh encoder 2,8 × 1,5 mm cũng phải giữ trong file PTH.

Encoder SW1 cần mua đúng kích thước chân/lỗ/rãnh đã dùng; model STEP EC11 trong dự án chỉ là model minh họa tương thích, không chứng nhận model linh kiện mua thực tế. Khi đặt PCB, chọn rõ surface finish vì job hiện để `None`.

## Phạm vi chức năng của nguyên mẫu

Đã đối chiếu pinout/topology auto-reset, USB ESD, microSD và AMS1117; không tìm thấy lỗi nối chân mới. [Báo cáo điện và nguồn sơ cấp](ELECTRICAL_REVIEW.md) trình bày chi tiết:

- **SMA/ANTEN:** chân trung tâm hiện là NC; không có đường RF tới module. Phiên bản WROOM-1 dùng antenna tích hợp. Nếu cần cổng SMA làm antenna ngoài thì chức năng đó hiện chưa được thực hiện trên PCB này.
- **Nguồn 3,3 V:** Q3 không tạo cách ly chống cấp ngược khi đã mở; nguồn board có thể đi ra J4.6. Với nguyên mẫu cấp USB, không đồng thời đấu một nguồn 3,3 V khác vào J4.6. Tài liệu dự án xác định J4.6/J4.7 là đầu vào nguồn, không phải VTref.
- **Nguồn và nhiệt:** cần thử AMS1117 sau diode SS34 với tải Wi-Fi + SD thực tế, điện áp USB/cáp và nhiệt độ. Chưa có số đo để khẳng định thiếu nguồn hoặc quá nhiệt, cũng chưa có số đo chứng minh đủ biên trong mọi điều kiện.
- **USB:** chưa xác nhận trở kháng vi sai bằng stackup nhà máy/solver/TDR và chưa thử chức năng USB trên PCB lắp ráp.
- **SD:** socket và R33/R34/R36–R41/C15/C16 giữ DNP từ bản gốc; nếu dùng thẻ nhớ phải lắp đủ. Bản vẽ TF-01A có công tắc detect về vỏ GND và mapping hiện tại phù hợp.

## Các cảnh báo không phải lỗi đi dây mới

- 6 cảnh báo footprint khác bản thư viện: R1, R5, R6, R25, TYPE-C, ESP32-S3. Đây là footprint tùy chỉnh đã có trước; các pad/hình học không đổi sau sửa chữ.
- 20 cảnh báo schematic parity là 10 missing + 10 extra do PCB dùng tên chức năng thay J1/U1/...; đối chiếu UUID và 207 kết nối chân đã xác nhận không sai net.
- Viền đồ họa footprint có nét 0,12 mm được giữ nguyên; nét chữ đều ≥ 0,15 mm. Ba nét outline antenna ESP32 ngoài mép board sẽ bị cắt khi gia công; không có nhãn chức năng connector nằm ngoài board.

Thư mục `cam/` là bộ xuất mới để kiểm tra, chưa gửi hoặc đặt tại nhà máy. Sau khi hai điểm cơ khí/gia công được xác nhận, có thể dùng đúng bản PCB này để chốt một bộ Gerber/drill cho đơn nguyên mẫu. Không dùng lại Gerber REV1.0 của `layout_manual` cho phiên bản này.
