# Kiểm tra lại sau khi sửa gốc — 09/11/2026

**Đạt kiểm tra tính đầy đủ, đồng bộ hình học và gốc tọa độ của bộ file xuất. Lỗi lệch gốc Gerber–drill ở lần kiểm tra trước đã được sửa.**

Đối tượng: `D:\esp32_debug\03_pcb\layout_manual\Gerber_REV1.0`. Gerber được xuất lúc 10:12:33, drill lúc 10:13:48 ngày 09/11/2026, giờ Việt Nam/Thái Lan.

## Kết quả

| Kiểm tra | Kết quả |
|---|---|
| Thành phần bộ file | Đủ 9 Gerber + 2 drill + 1 job |
| Đường bao | 70 × 60 mm, kín, R3; tọa độ X=0…70, Y=0…60 mm |
| PTH | 158 vị trí: 152 lỗ tròn và 6 rãnh |
| NPTH | 8 lỗ, trong đó bốn lỗ bắt vít Ø3,2 mm |
| Trùng tâm PTH với đồng | Cả 158 vị trí khớp flash pad/via trên F.Cu và B.Cu; sai khác tâm lớn nhất 0,0005 mm do làm tròn file drill |
| Đối chiếu PCB đang lưu | Cả 9 lớp Gerber khớp hình học với bộ xuất đối chiếu mới; hai file drill khớp vị trí, kích thước và loại lỗ/rãnh |
| DRC sau refill zone | 0 error, 0 kết nối hở, 8 cảnh báo |
| Net schematic–PCB theo UUID và số chân | Đã kiểm tra lại 207 kết nối chân: không thiếu component và không có net sai khác |

Không áp dụng phép dịch, mirror hoặc tự căn drill khi kiểm tra bộ file mới. Ảnh kiểm tra thực tế:

- [Mặt trên, drill đúng tọa độ gốc](top_actual_alignment.png)
- [Mặt dưới, cùng hướng nhìn từ trên](bottom_actual_alignment.png)

## Các cảnh báo cũ còn lại

- 6 cảnh báo khác bản thư viện tại R1, R5, R6, R25, TYPE-C và ESP32-S3. Đây là footprint đã tùy chỉnh; vẫn cần bảo đảm chúng đúng linh kiện thực tế.
- 2 cảnh báo silkscreen chồng nhau tại AMS1117 và D7.
- 20 cảnh báo schematic parity gồm 10 missing + 10 extra vì Reference trên PCB đã đổi thành tên chức năng. Kiểm tra riêng theo UUID và số chân không phát hiện sai net.
- Nét silkscreen 0,12 mm và xác nhận trở kháng USB với stackup nhà máy vẫn là các điểm đã ghi ở báo cáo ban đầu; lần xuất lại không thay đổi thiết kế đó.

**Bộ file mới có thể gửi nhà máy kiểm tra CAM/DFM cho PCB trần; không còn cần sửa gốc tọa độ.** Kết quả này xác nhận bộ xuất khớp thiết kế đang lưu, chưa xác nhận toàn bộ chức năng mạch hoặc độ vừa linh kiện thực tế. Thông số thiết kế: PCB FR-4 2 lớp, 70 × 60 mm, dày 1,6 mm, đồng 35 µm mỗi mặt. Chọn surface finish thực tế khi đặt hàng.

Chỉ đóng gói các file trong `Gerber_REV1.0` khi gửi nhà máy. Các thư mục `review_Gerber_REV1.0` và `reference` là bằng chứng kiểm tra, không phải lớp gia công bổ sung.

Chi tiết máy đọc: [result.json](result.json), [drc.json](drc.json), [netlist_comparison.json](netlist_comparison.json). SHA-256 của cả 12 file đã được ghi nhận; quá trình kiểm tra không thay đổi nội dung bộ file người dùng vừa xuất.
