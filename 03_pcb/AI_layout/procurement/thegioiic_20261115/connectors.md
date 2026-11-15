# Đầu nối và cơ khí — Thegioiic, 15/11/2026

Đã đọc trực tiếp trang sản phẩm và tìm kiếm nội bộ của nhà bán. Giá/tồn bên dưới là ảnh chụp dữ liệu đang công bố khi tra cứu, không phải số trong kết quả Google cũ. Chưa thêm giỏ hàng hoặc đặt mua.

## Có thể dùng theo chuẩn chân

| Vị trí | Sản phẩm nhà bán | Mua cho 1 PCB | Giá/đơn vị | Tồn ghi nhận |
|---|---|---:|---:|---:|
| OLED, SWDIO | [IC22930 — Hàng Rào Đực Đơn 2.54mm 40 Chân 1 Hàng Cao 11.2mm Xuyên Lỗ Chân Đồng Mạ Niken](https://www.thegioiic.com/hang-rao-duc-don-2-54mm-40-chan-1-hang-cao-11-2mm-xuyen-lo-chan-dong-ma-niken) | 1 thanh, cắt đủ chân | 1,596 đ | 2,410 |
| UART | [IC14868 — Hàng Rào Đực Đôi 2.54mm 80 Chân 2 Hàng Cao 11.2mm Xuyên Lỗ](https://www.thegioiic.com/hang-rao-duc-doi-2-54mm-80-chan-2-hang-cao-11-2mm-xuyen-lo) | 1 thanh, cắt đủ chân | 1,938 đ | 3,120 |

Một thanh 1×40 cung cấp cả đoạn 1×4 OLED và 1×7 SWD; một thanh 2×40 cắt đoạn 2×3 UART. Tổng hai thanh: **3.534 đ** trước phí giao hàng.

## Cần đối chiếu trước khi mua

| Vị trí / số lượng | Ứng viên có hàng | Giá | Tồn | Kết luận |
|---|---|---:|---:|---|
| TYPE-C / 1 | [IC36686 — Cổng USB Cái 3.1 Type C 16 Chân Thẳng Dán SMD](https://www.thegioiic.com/cong-usb-cai-3-1-type-c-16-chan-thang-dan-smd) | 1,710 đ | 17 | Khác MPN; chưa xác nhận khớp chân vỏ/land pattern. |
| BOOT, RESET / 2 | [IC3592 — Nút Nhấn 3x6mm Cao 2.5mm 2 Chân SMD Đầu Trắng](https://www.thegioiic.com/nut-nhan-3x6mm-cao-2-5mm-2-chan-smd-dau-trang) | 570 đ | 2,559 | Ứng viên 2 chân; MOQ10; pad/chiều rộng chân khác, chưa chốt thay trực tiếp. |
| SW1 / 1 | [IC19056 — EC11 Rotary Encoder 20 Vị Trí, Trục D Dài 20mm](https://www.thegioiic.com/ec11-rotary-encoder-20-vi-tri-truc-d-dai-20mm) | 12,540 đ | 903 | Bản vẽ gá có khoảng cách tai12,5mm; PCB11,2mm. Không chốt cho PCB đã đặt. |
| SD SOCKET / 1 | [IC4602 — MiSD-PP-9P-V1 Khay Thẻ Nhớ MicroSD 9 Chân Push-Push](https://www.thegioiic.com/misd-pp-9p-v1-khay-the-nho-microsd-9-chan-push-push) | 2,508 đ | 92 | Tùy chọn/DNP; giống pitch nhưng chưa xác nhận chân vỏ/land pattern TF-01A. |
| ANTEN / 1 | [IC5095 — SMA-KWE-14.5 Đầu RF SMA Đực Dài 14.5mm Vuông Góc 90 Độ Hàn PCB](https://www.thegioiic.com/sma-kwe-14-5-dau-rf-sma-duc-dai-14-5mm-vuong-goc-90-do-han-pcb) | 11,400 đ | 325 | Tùy chọn/DNP/NC; không cần mua để có RF, chưa khớp MPN. |

Tìm kiếm nội bộ đúng mã GT-USB-7010ASV, PTS636, TF-01A và BWSMA trả về 0 sản phẩm. Điều này chỉ có nghĩa là chưa tìm thấy qua các truy vấn này, không khẳng định nhà bán tuyệt đối không có hàng.

## Bằng chứng kích thước

- BOOT/RESET thực tế có **2 pad**, tâm cách7,75mm, mỗi pad1,25×1,0mm; thân C&K6×3,5mm, cao2,5mm. [Bản vẽ nút OEM](https://file.thegioiic.com/upload/large/53121.jpg) cho thân6×3mm, dài chân tổng8±0,1mm, rộng chân1,3mm. Có thể cân nhắc hàn thủ công sau kiểm tra mẫu, không đánh dấu khớp footprint.
- [Bản vẽ encoder OEM](https://file.thegioiic.com/upload/large/50639.jpg) là cơ sở không duyệt EC11 chỉ theo tên; native PCB giữ các lỗ/tai của footprint Alps.
- [Bản vẽ microSD OEM](https://file.thegioiic.com/upload/large/4306.jpg) có pitch1,1mm/guide8mm nhưng chưa có land pattern đầy đủ để xác nhận toàn bộ pad vỏ và CD.
- USB được nhà bán gắn [tài liệu Shouhan TYPE-C16PIN2MD(073)](https://file.thegioiic.com/upload/documents/1734591720_TYPE-C-16PIN.pdf), khác mã G-Switch trên PCB. Số chân16 không đủ chứng minh thay trực tiếp.

Các tọa độ, kích thước pad/lỗ lấy trực tiếp từ PCB nằm trong `native_connectors_geometry.json`; dữ liệu và liên kết đầy đủ nằm trong `connectors.json`.
