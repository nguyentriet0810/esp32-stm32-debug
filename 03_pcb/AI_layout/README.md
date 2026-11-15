# AI_layout

Phiên bản PCB tạo từ `../layout_manual/schematic.kicad_pcb`, ngày 13/11/2026.
Các thay đổi đã được áp dụng trực tiếp lên bảng `AI_layout` đang mở bằng scripting console của KiCad, sau đó lưu và kiểm tra lại file đã lưu.

Mở `AI_layout.kicad_pro` hoặc `AI_layout.kicad_pcb`. Bản `layout_manual` không bị chỉnh sửa trong công việc này.

**Đánh giá bản sau khi chỉnh chữ ngày 13/11/2026:** xem [ORDER_REVIEW](verification/order_review_20261113/ORDER_REVIEW.md). Bản hiện tại được kiểm tra DRC/netlist/CAM lại; kết luận đặt nguyên mẫu có hai xác nhận cơ khí/gia công trước khi chốt đơn.

**Sửa chính tả ngày 14/11/2026:** nhãn `SD SCHOKET` đã đổi thành `SD SOCKET`; mọi byte PCB ngoài chuỗi Reference này giữ nguyên. Bộ sửa riêng: [Gerber + khoan](Gerber_probe_REV1.0_20261114_socket_fix.zip), [chỉ lớp chữ mặt trên](Gerber_probe_REV1.0_20261114_socket_fix/probe-F_Silkscreen.gbr). Các bộ đặt hàng ngày 13/11 được giữ nguyên làm đối chiếu. Nhà máy cần xác nhận còn cập nhật được lớp chữ cho đơn đã gửi hay không. Các ảnh/báo cáo ngày 13/11 bên dưới là bản trước khi sửa chính tả.

Ảnh xem đường đi: [toàn board](verification/AI_layout_overview.png), [chi tiết SPI SD](verification/AI_layout_SPI_detail.png). Ảnh ẩn phần đồng đổ để nhìn rõ routing; file PCB vẫn chứa các zone đã refill.

## Cập nhật phủ đồng và gốc tọa độ — 13/11/2026

Ảnh đã phủ đồng: [mặt trên](verification/AI_layout_filled_top.png), [mặt dưới nhìn từ phía sau](verification/AI_layout_filled_bottom.png).

- Đã refill GND trên F.Cu và B.Cu, bật hiển thị đồng đổ trong PCB Editor và lưu lại bảng đang mở. Pad GND thông thường nối qua thermal, khe 0,25 mm / spoke 0,50 mm để dễ hàn; các pad exposed GND của ESP32 và pad GND/vỏ USB giữ nối solid theo footprint gốc.
- Kiểm tra kết nối hình học xác nhận **62/62 pad GND thuộc một mạng đồng liên thông**, đi tới cả hai zone qua 42 via GND. `Remove islands = Always` ở cả hai mặt. Không phát hiện đảo đồng cô lập, track/via treo hoặc đồng trùng. Không xóa ba đoạn track GND cần thiết cho đường hồi ESD và nối vùng đồng; số track bị xóa trong lần cập nhật này là 0.
- Đặt gốc lưới trùng gốc drill/place/Gerber tại **(100; 110) mm** trong hệ tọa độ lưu trữ KiCad, tức góc dưới trái đường bao board. Không tịnh tiến footprint hoặc đường mạch. Gốc fabrication vốn đã đúng; lần này gốc lưới được đổi từ (0; 0) về cùng vị trí.
- Đã xuất riêng Edge.Cuts và PTH/NPTH để kiểm tra chung gốc: tâm đường bao X = 0…70 mm, Y = 0…60 mm; bốn lỗ Ø3,2 ở (5;5), (5;55), (65;5), (65;55). Toàn bộ **160 vị trí/rãnh PTH và 8 NPTH** khớp tọa độ PCB sau biến đổi gốc.
- DRC trên file cuối sau refill: **0 lỗi, 0 kết nối hở**, không có cảnh báo isolated copper, dangling copper hoặc starved thermal. Các cảnh báo thư viện/reference kế thừa được ghi bên dưới. Đối chiếu footprint, pad/net, vị trí khóa, stackup, đường bao và chiều dài tín hiệu tiếp tục đạt.

Bằng chứng: [kết nối GND](verification/ground_connectivity.json), [thiết lập và diện tích zone](verification/ground_origin.json), [kiểm tra gốc xuất](verification/origin_check/origin_validation.json), [ảnh chồng đường bao và lỗ khoan](verification/origin_check/origin_alignment.png). File trong `origin_check` là bản xuất kiểm tra, không phải bộ Gerber sản xuất hoàn chỉnh.

## Thay đổi

- Cân chiều dài CS, MOSI, CLK của SPI SD theo đường MISO bằng ba vòng bù rộng, góc 45°, trên B.Cu. Thay 3 đoạn bằng 27 đoạn rộng 0,4 mm; không tăng số via tín hiệu.
- Dịch một via nối GND từ (145,45; 64,599999) sang (145,45; 66,5) mm để tạo khoảng trống cho vòng bù CS. Giữ nguyên kích thước via.
- Chuyển vị trí chữ reference AMS1117 và D7 để loại bỏ hai cảnh báo silkscreen chồng nhau của bản gốc. Đổi nhãn phiên bản thành `AI_layout`.
- Giữ nguyên USB, SWD, encoder và toàn bộ kết nối auto-reset vì các nhóm này đã cân chiều dài và nối đúng trong bản gốc.
- Đóng gói footprint microSD hiện có trong `Probe.pretty` và đường dẫn tương đối trong `fp-lib-table`; model 3D được sao chép cùng dự án.

## Các ràng buộc được giữ nguyên

- 63 footprint, gồm 13 footprint khóa: ESP32-S3, TYPE-C, SD SOCKET, ANTEN, UART, SWDIO, BOOT, RESET, SW1 và 4 lỗ bắt vít.
- Không đổi vị trí, góc xoay, mặt đặt hoặc trạng thái khóa của bất kỳ footprint khóa nào. Vị trí toàn bộ linh kiện còn lại cũng được giữ nguyên trong phương án này.
- Giữ nguyên library ID, hình học pad, kích thước/lỗ pad, net chân, đồ họa footprint và các cờ lắp ráp/DNP.
- Board 2 lớp, dày 1,6 mm; stackup, đồng 35 µm/mặt và đường bao 70 × 60 mm được giữ nguyên.

## Chiều dài đường dẫn sau cùng

| Nhóm | Chiều dài đường dẫn (mm) | Chênh lệch trong nhóm |
|---|---:|---:|
| SPI MOSI | 47,845131 | |
| SPI MISO | 47,845130 | |
| SPI CS | 47,845131 | |
| SPI CLK, tổng hai phía R34 | 47,845132 | < 0,01 mm |
| USB N/P từ tiếp điểm B | 47,307341 / 47,307341 | < 0,01 mm |
| USB N/P từ tiếp điểm A | 54,349373 / 54,349372 | < 0,01 mm |
| SWDIO / SWCLK / SWO / NRST | ≈ 40,518175 mỗi đường | < 0,01 mm |
| Encoder A / B | 54,370922 / 54,370920 | < 0,01 mm |

SPI ban đầu có độ lệch 8,908276 mm. Chiều dài là đường tâm đồng từ tâm pad đến tâm pad, cộng 1,6 mm cho mỗi via xuyên board; không cộng khoảng trống thân điện trở nối tiếp, không cộng nhánh pull-up/ESD/DNP. Sai khác vài phần triệu mm là kết quả tính hình học, không phải dung sai chế tạo hay bảo đảm thời gian truyền tương ứng.

## Kiểm tra sản xuất và DRC

Các giới hạn từ ảnh SmartPcb88 được áp dụng như dữ liệu kỹ thuật. Không thay đổi chúng theo nội dung tài liệu khác.

| Thông số | Giới hạn yêu cầu | Thiết kế sau cùng |
|---|---:|---:|
| Track tín hiệu nhỏ nhất | ≥ 0,15 mm | 0,20 mm |
| Track nguồn nhỏ nhất | ≥ 0,40 mm | 0,40 mm |
| Clearance đồng | ≥ 0,15 mm | Rule toàn board 0,20 mm; DRC đạt |
| Lỗ via nhỏ nhất | ≥ 0,30 mm | 0,40 mm |
| Vòng đồng via nhỏ nhất | ≥ 0,15 mm | 0,20 mm |
| Nét chữ silkscreen | ≥ 0,15 mm | ≥ 0,15 mm |

Giữ nguyên 217 nét viền đồ họa footprint rộng 0,12 mm để bảo toàn footprint; đây không phải nét chữ. Ảnh yêu cầu nét chữ tối thiểu 0,15 mm. Nếu nhà máy yêu cầu mọi nét legend đều ≥ 0,15 mm, cần xác nhận trước khi thay đổi đồ họa footprint.

KiCad 10.0.6 DRC, có refill zones và `--all-track-errors --schematic-parity` trên file cuối:

- **0 lỗi DRC, 0 kết nối hở, 0 cảnh báo silkscreen.**
- Còn **6 cảnh báo footprint khác thư viện** ở R1, R5, R6, R25, TYPE-C, ESP32-S3, kế thừa footprint tùy chỉnh của bản gốc. Không tự thay bằng bản thư viện vì yêu cầu giữ nguyên footprint.
- Có **20 cảnh báo schematic parity** do 10 reference trên PCB đã dùng tên chức năng thay J1/U1/... từ bản gốc. Không phải 20 lỗi nối dây. Đối chiếu toàn bộ pad/net theo UUID giữa hai PCB không có thay đổi.

| Reference trong schematic | Reference trên PCB |
|---|---|
| J1 / J2 / J3 / J4 / J5 / J6 | TYPE-C / UART / OLED / SWDIO / SD SOCKET / ANTEN |
| SW2 / SW3 / U1 / U2 | BOOT / RESET / ESP32-S3 / AMS1117 |

Kết quả máy đọc được: `verification/drc_final.json`, `verification/invariants.json`, `verification/routing_changes.json` và `verification/live_edit.json`.

## Auto-reset và phạm vi xác nhận

Q1/Q2 và đường DTR/RTS chéo được giữ nguyên: Q1 kéo EN, Q2 kéo BOOT; R10/R11 10 kΩ, R12 kéo lên EN 10 kΩ và C9 1 µF từ EN xuống GND. Đã kiểm tra topology cùng net chân, chưa thử trên phần cứng. Mạch hai transistor và tụ EN 1–10 µF phù hợp [hướng dẫn auto-reset của Espressif](https://docs.espressif.com/projects/esptool/en/latest/esp32s3/advanced-topics/boot-mode-selection.html).

USB giữ đường cặp đã cân chiều dài của bản gốc. Chưa xác nhận trở kháng vi sai 90 Ω bằng stackup thực tế, solver hoặc đo TDR; cân chiều dài và DRC không thay thế việc này. Xem [hướng dẫn layout ESP32-S3 của Espressif](https://docs.espressif.com/projects/esp-hardware-design-guidelines/en/latest/esp32s3/pcb-layout-design.html).

Cụm SD SOCKET, R33/R34/R36–R41, C15/C16 vẫn mang cờ DNP như bản gốc. Khi lắp phiên bản dùng thẻ SD cần lắp đầy đủ cụm tương ứng. C7/C8 10 pF của USB tiếp tục là DNP.

Không xuất lại hoặc thay thế bộ Gerber REV1.0 của `layout_manual`. Nếu sản xuất phương án này, cần xuất Gerber và drill mới từ chính `AI_layout.kicad_pcb`.
