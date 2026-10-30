# Kế hoạch phát triển

Trạng thái: **bản đề xuất để thống nhất trước khi triển khai thêm firmware**.
Ngày lập: 30/10/2026.

## 1. Mục tiêu và phạm vi bản đầu

Xây dựng probe debug STM32 dùng ESP32-S3 N16R8 và ESP-IDF v6.1, có cấu trúc dễ bảo trì,
kiểm chứng được trên phần cứng và quản lý đầy đủ bằng Git.

**MVP v0.1** tập trung vào debug và nạp firmware qua Wi-Fi bằng GDB:

- Kết nối target qua SWD; nhận diện MCU và báo nguyên nhân khi không kết nối được.
- Halt, run, single-step, hardware breakpoint, đọc thanh ghi và đọc/ghi bộ nhớ.
- Erase/program/verify flash theo driver hỗ trợ đúng model STM32.
- Quản lý một phiên điều khiển target tại một thời điểm; có timeout và phục hồi phiên.
- OLED SSD1306 128×64 và rotary encoder để xem trạng thái, thao tác menu và cấu hình probe khi rảnh.
- Có hướng dẫn sử dụng, cấu hình kết nối GDB và ma trận phần cứng đã kiểm chứng.

Các mốc sau bổ sung đọc RAM liên tục cho MCUViewer, SWO và USB/CMSIS-DAP.
Chưa cam kết hỗ trợ toàn bộ STM32; mỗi model được công bố cần có trạng thái kiểm chứng.
Chưa đặt chỉ tiêu tốc độ tối đa hoặc ngày hoàn thành trước khi có board và số đo nền.

## 2. Hiện trạng

| Hạng mục | Trạng thái |
|---|---|
| Cấu trúc `01_source`, `02_document`, `README.md` | Đã có |
| Repo private và `.gitignore` | Đã có |
| ESP-IDF v6.1, Python, compiler và công cụ cục bộ | Đã có |
| SDK submodule, lock file và hướng dẫn setup | Đã có |
| Build firmware nền cho N16R8 | Đã thành công |
| Component và quy chuẩn C | Có `probe_board`; C17, comment Doxygen, hướng dẫn MISRA C:2025 |
| Kiểm tra flash/PSRAM/nguồn trên board thật | Chưa thực hiện |
| Khôi phục project trên một máy Windows sạch | Chưa thực hiện |
| SWD, Wi-Fi, GDB server, MCUViewer, SWO, USB probe | Chưa triển khai |
| OLED SSD1306 128×64 và rotary encoder | Đã bổ sung yêu cầu; chưa triển khai |

### Phần cứng người dùng đang có

Probe là **ESP32-S3 N16R8 dạng DevKit có chân đưa ra sẵn**. Sơ đồ chân sử dụng thực tế
sẽ được đối chiếu với board trước khi đấu nối.

| Target | Thứ tự đề xuất | Vai trò nghiệm thu |
|---|---|---|
| STM32F103 | 1 | Kết nối SWD đầu tiên, đọc RAM, debug và nạp flash nền |
| STM32F411 | 2 | Target thứ hai của MVP, kiểm tra hỗ trợ khác dòng F1/F4 |
| STM32H503 | 3 | Mở rộng sau nền F1/F4, đánh giá driver, debug và flash theo model |
| STM32C562 | 4 | Đánh giá riêng debug port và driver flash trước khi đưa vào ma trận hỗ trợ |

STM32C562 thuộc dòng STM32C5 theo [tài liệu ST](https://www.st.com/en/microcontrollers-microprocessors/stm32c562ce.html).
Khả năng truy cập SWD và khả năng nạp flash được kiểm tra riêng; không suy ra hỗ trợ C562
từ driver STM32C0 hoặc từ việc nhận diện được lõi ARM.
Mã đầy đủ/package của từng chip, probe tham chiếu và dụng cụ đo hiện có sẽ được ghi ở P0.

Giao diện cục bộ gồm OLED 0,96 inch I2C SSD1306 128×64 đã được xác nhận và rotary encoder
A/B với nút nhấn được đề xuất. Pin map, menu và điều kiện nghiệm thu nằm trong
[Phần cứng và giao diện tại probe](05_hardware_and_local_ui.md); model encoder còn cần xác nhận.

## 3. Kiến trúc đề xuất

Luồng chính: **GDB trên PC → TCP/Wi-Fi → dịch vụ probe → Black Magic Debug → SWD → STM32**.

| Phần | Trách nhiệm |
|---|---|
| Board configuration | Chân GPIO, nRST, nguồn và cấu hình riêng của board |
| SWD platform port | Dedicated GPIO, timing, đổi chiều SWDIO và xử lý giao dịch SWD |
| Debug backend | Tích hợp Black Magic Debug, driver target và thao tác flash |
| Probe service | Sở hữu SWD, tuần tự hóa yêu cầu, timeout và vòng đời phiên |
| Network transport | Wi-Fi, TCP, cấu hình mạng và phát hiện mất kết nối |
| Local UI | OLED, encoder/nút nhấn, xem trạng thái và gửi yêu cầu cấu hình qua dịch vụ probe |
| Trace/acquisition, giai đoạn sau | Đọc RAM định kỳ, buffer PSRAM, nhận SWO và giao thức MCUViewer |
| Application | Khởi tạo và ghép các thành phần |

Đề xuất giữ Wi-Fi/network ở core 0 và một task sở hữu SWD ở core 1. Các giao diện host
không trực tiếp điều khiển SWD đồng thời. Giữ mã cần timing trong bộ nhớ nội phù hợp;
PSRAM dùng cho buffer lớn sau khi đo yêu cầu bộ nhớ và giới hạn truy cập.
UI chạy ưu tiên thấp ở core 0, nhận sự kiện qua queue; không gọi SWD trực tiếp hoặc
truyền OLED trong ISR/task sở hữu SWD.

Black Magic Debug được ghim phiên bản/commit và sử dụng driver sẵn có. Lớp SWD cần tích hợp
với backend, tránh duy trì hai bản triển khai giao thức SWD độc lập. Nếu tái sử dụng port
từ dự án tham khảo, giữ thông tin tác giả và giấy phép liên quan.

Các module chỉ được tạo khi có triển khai thực tế trong `01_source/components`.
Dependency upstream đặt trong `01_source/external` hoặc theo cơ chế component đã chọn.
Component `probe_board` hiện tách khỏi `main`; các component tiếp theo tuân theo
[quy chuẩn C](06_coding_standard.md) và [hồ sơ ngoại lệ nền tảng](07_misra_deviations.md).
Mã gốc dự án dùng GPL-3.0-or-later, giữ khả năng tích hợp backend Black Magic Debug.

## 4. Các giai đoạn và điều kiện hoàn thành

### P0 — Chốt yêu cầu và bộ thử nghiệm

- Đối chiếu pinout DevKit S3 N16R8, cổng USB/COM, nguồn và chân có thể sử dụng.
- Ghi mã đầy đủ của F103/F411/H503/C562; chọn F103 làm target đầu tiên và F411 làm target thứ hai của MVP.
- Đánh giá phạm vi driver cho H503 và C562; tách việc bổ sung driver nếu backend được chọn chưa hỗ trợ.
- Chốt pin map SWDIO/SWCLK/nRST, logic 3.3 V cho nguyên mẫu và cách nối nguồn/GND.
- Giữ chân debug mẫu GPIO4/5/6/10; đề xuất OLED SDA/SCL GPIO8/9, encoder A/B/SW GPIO11/12/13.
- Xác nhận model encoder, pull-up/nguồn OLED và cách thao tác menu; giữ GPIO21 và GPIO0/BOOT theo chức năng tham chiếu.
- Chọn phiên bản Black Magic Debug; quyết định tái sử dụng phần nào của port tham khảo.
- Chốt hành vi detach, mất TCP, reset và lỗi flash; phân biệt mất kết nối với lệnh chủ động của GDB.
- Chốt danh sách chức năng MVP và cách đánh dấu model đã thử/chưa thử.

**Đầu ra:** yêu cầu MVP, sơ đồ nối dây, danh sách thiết bị thử và quyết định tích hợp backend.
**Nghiệm thu:** có đủ thông tin để đấu nối và viết port; các lựa chọn còn thiếu được ghi rõ.

### P1 — Xác nhận nền tảng trên phần cứng

- Nạp firmware nền, kiểm tra log IDF, CPU, flash 16 MB và PSRAM 8 MB.
- Kiểm tra reset/cấp nguồn lại và độ ổn định bộ nhớ khi vận hành bình thường.
- Kiểm chứng quy trình clone/setup/build trên môi trường Windows sạch.
- Thiết lập kiểm tra build bằng SDK đã ghim và quy ước review thay đổi trước khi merge.
- Áp dụng C17/warnings/analyzer cho mã dự án; tiếp tục hoàn thiện checker và guideline coverage
  trước khi đánh dấu mức tuân thủ MISRA.

**Đầu ra:** log kiểm chứng board, hướng dẫn setup đã thử và cấu hình board có phiên bản.
**Nghiệm thu:** board khởi động đúng profile, không có reset ngoài dự kiến trong bài thử;
một môi trường sạch build được theo tài liệu. Build hiện có mới hoàn thành phần kiểm tra trên máy phát triển.

### P1a — OLED và rotary encoder

- Đối chiếu pin map thực tế, xác minh địa chỉ I2C và hiển thị thông tin board trên SSD1306.
- Đọc A/B/nút nhấn, xử lý dội, xác định số cạnh mỗi nấc và chạy menu xoay/chọn/quay lại.
- Có cấu hình độ tương phản, thời gian tắt màn hình và thông số mặc định ở trạng thái rảnh.
- Thử thiếu/lỗi OLED, cấp nguồn lại và thao tác encoder nhanh/chậm; mọi chờ I2C có timeout.
- Tách task UI khỏi SWD và thiết kế giao diện trạng thái/command trước khi ghép vào debug.

**Đầu ra:** giao diện cục bộ hoạt động trên nguyên mẫu, thông tin đấu nối và cấu hình module.
**Nghiệm thu:** hiển thị đúng, thao tác menu ổn định, phân biệt nhấn ngắn/giữ và lỗi OLED
không làm treo probe. Trạng thái mạng/target thật sẽ được ghép ở P3/P4.

### P2 — SWD cơ bản, kiểm chứng cục bộ

- Chuẩn bị firmware thử cho STM32 với pattern RAM/counter biết trước; nạp bằng probe tham chiếu
  hoặc bootloader phù hợp để có dữ liệu đối chiếu trước khi probe mới hỗ trợ nạp flash.
- Tích hợp port SWD tối thiểu với API thấp của backend; bắt đầu ở 100–400 kHz.
- Đọc DPIDR, nhận diện MEM-AP và đọc một vùng RAM có dữ liệu biết trước.
- Kiểm tra chiều SWDIO, turnaround, parity, ACK WAIT/FAULT và timeout.
- Thử không có target, tháo dây, target reset, nối lại; không để task chờ vô hạn.
- Đối chiếu dữ liệu với một probe tham chiếu nếu có; đo tín hiệu bằng logic analyzer nếu có.

**Đầu ra:** lớp SWD hoạt động, cách chẩn đoán kết nối và bằng chứng đọc đúng dữ liệu.
**Nghiệm thu:** đọc lặp ổn định ở tốc độ nền, báo lỗi hữu hạn khi mất target và kết nối lại được.
Chưa tăng tốc khi kết nối nền còn lỗi.

### P3 — Wi-Fi, TCP và quản lý phiên

- Lưu cấu hình mạng cục bộ, xác định IP và triển khai TCP transport cho GDB.
- Tách task mạng khỏi task sở hữu SWD, quy định timeout và dung lượng buffer.
- Chỉ một client sở hữu target; client thứ hai nhận trạng thái từ chối/bận rõ ràng.
- Thử TCP chia nhỏ/gộp dữ liệu, client đóng đột ngột, mất Wi-Fi và kết nối lại.
- Không đưa thông tin đăng nhập mạng vào mã nguồn hoặc commit.
- Đưa trạng thái mạng, IP và phiên TCP thật lên OLED qua snapshot trạng thái.

**Đầu ra:** transport và vòng đời phiên được kiểm chứng, cấu hình kết nối từ PC.
**Nghiệm thu:** mất mạng không làm treo task SWD hoặc tạo phiên cũ chiếm tài nguyên;
target tuân theo hành vi đã chốt ở P0.

### P4 — GDB debug và nạp flash

- Ghép GDB server của Black Magic Debug với transport và SWD port.
- Kiểm tra scan/attach/detach, halt/run, step, breakpoint, thanh ghi và bộ nhớ.
- Thử erase/program/verify với firmware thử nghiệm của đúng target, rồi reset và chạy lại.
- Kiểm tra lỗi nạp, mất target giữa thao tác và khả năng phục hồi kết nối.
- Tạo ví dụ GDB command và cấu hình cho một IDE được chọn để nghiệm thu.
- Hiển thị target, clock SWD thực tế, phiên debug và lỗi lên OLED; khóa thao tác UI xung đột với host.

**Đầu ra:** probe sử dụng được với GDB trên target đầu tiên, hướng dẫn có thể làm lại.
**Nghiệm thu:** hoàn thành một chu trình build firmware target → nạp → debug → reset/run;
verify flash đúng, các lỗi có thông báo và đường phục hồi rõ ràng.

### P5 — Độ ổn định, nhiều dòng STM32 và phát hành MVP v0.1

- Nghiệm thu MVP trên F103 và F411; sau đó mở rộng kiểm chứng H503 và C562 theo kết quả đánh giá driver.
- Lập ma trận theo model: nhận diện, attach, RAM, flash, reset, breakpoint và lỗi còn lại.
- Mở rộng kiểm chứng Cortex-M0/M0+, M3/M4, M7 và M33 theo phần cứng và driver có sẵn;
  lưu ý cache, nhiều lõi, debug port và cấu hình bảo mật theo từng target.
- Đo tốc độ đọc/ghi RAM, nạp flash, độ trễ và mức dùng RAM/CPU; tăng SWD theo từng mức.
- Chạy phiên dài với số lần kết nối lại xác định trước; theo dõi heap, watchdog và timeout.
- Đo ảnh hưởng cập nhật OLED và thao tác encoder khi debug; kiểm tra lệnh UI tuân theo quyền sở hữu target.
- Hoàn thiện tài liệu đấu nối, hướng dẫn, giới hạn hỗ trợ và ghi chú phiên bản.

**Đầu ra:** release MVP v0.1 và ma trận model đã kiểm chứng.
**Nghiệm thu:** các chức năng MVP qua bài thử trên các model công bố; có kết quả đo và
giới hạn được ghi lại. Model chưa thử không được gắn nhãn đã hỗ trợ thực tế.

### P6 — Đọc RAM liên tục và MCUViewer

- Tích hợp giao thức NETWORK PROBE tương thích phiên bản MCUViewer được chọn.
- Có kế hoạch lấy mẫu, timestamp, đọc theo khối và ring buffer trong PSRAM.
- Báo mất mẫu, tràn buffer và mất target; xử lý backpressure từ mạng/client.
- Thử bằng biến có tín hiệu biết trước trong firmware target; phân biệt số đo khi CPU vẫn chạy
  với một snapshot đồng bộ, không giả định mọi biến được đọc cùng thời điểm.
- Khóa quyền truy cập giữa phiên GDB và phiên acquisition.
- Nếu cần điều chỉnh thông số STM32 bằng encoder, đặc tả danh sách biến/kiểu/giới hạn/bước
  và chính sách áp dụng qua probe service trước khi triển khai ghi tham số.

**Đầu ra:** bản v0.2 theo dõi biến và vẽ đồ thị bằng MCUViewer.
**Nghiệm thu:** đối chiếu đúng dữ liệu thử, có số đo tốc độ/jitter/mất mẫu và phục hồi sau gián đoạn.

### P7 — SWO và trace

- Chọn target có khả năng trace cần thử; xác định chân SWO, encoding và clock phù hợp.
- Nhận dữ liệu, buffer và chuyển lên host; chỉ rõ bên nào cấu hình ITM/TPIU và bên nào giải mã.
- Kiểm tra dữ liệu biết trước, tràn buffer và tải đồng thời với acquisition.

**Đầu ra:** bản trace đã có dữ liệu kiểm chứng và giới hạn baud rate đo được.
**Nghiệm thu:** dữ liệu trace đúng trong dải đã công bố; mất dữ liệu được báo và định lượng.

### P8 — USB/CMSIS-DAP và phần cứng mở rộng

- Đánh giá nhu cầu CMSIS-DAP, CDC và công cụ host/IDE cần tương thích.
- Thiết kế đường CMSIS-DAP dùng chung lớp SWD và quyền sở hữu target; không giả định
  giao thức CMSIS-DAP dùng nguyên giao diện GDB hoặc driver flash chạy trên probe.
- Kiểm chứng USB bằng host được chọn; xử lý cạnh tranh phiên USB và Wi-Fi.
- Sau khi nguyên mẫu ổn định mới chốt PCB, VTref, chuyển mức, reset và bảo vệ chân theo dải điện áp cần hỗ trợ.

**Đầu ra:** phương án USB và phần cứng dựa trên nhu cầu đã xác nhận.
**Nghiệm thu:** các host/target được chọn qua bài thử; phạm vi phiên bản sẽ chốt sau MVP.

## 5. Thứ tự triển khai và quản lý công việc

Thứ tự chính: **P0 → P1 → P1a → P2 → P3 → P4 → P5 → P6 → P7/P8**.
P7 và P8 chỉ được ưu tiên sau khi có nhu cầu rõ; lỗi SWD/debug của MVP phải được xử lý trước.

- Mỗi hạng mục có phạm vi, đầu ra, điều kiện nghiệm thu và kết quả kiểm tra.
- Thay đổi theo nhánh `feature/...`, `fix/...` hoặc `docs/...`, review trước khi merge vào `main`.
- Commit ghi rõ thay đổi; cập nhật tài liệu khi đổi chân, giao thức hoặc cách build.
- Mã firmware thử nghiệm, script PC và bài kiểm tra nằm trong `01_source` khi được tạo.
- Kết quả đo, ma trận target và hướng dẫn nghiệm thu nằm trong `02_document`.
- Trạng thái phân biệt: chưa làm, đang làm, đã build, đã thử trên board, đã nghiệm thu.
- Bài thử tự động tập trung vào codec, state machine, timeout và tình huống lỗi có ý nghĩa;
  việc timing SWD, flash và trace cần xác nhận bằng phần cứng.

## 6. Công việc kế tiếp ngay sau khi thống nhất kế hoạch

1. Hoàn tất P0: ghi mã/pinout board S3 và F103/F411, xác nhận probe tham chiếu và dụng cụ đo đang có.
2. Chốt pin map debug/OLED/encoder và các chức năng MVP; ghi quyết định chọn backend/port.
3. Thực hiện P1 và P1a trên board thật rồi triển khai SWD theo tốc độ nền.

Các giai đoạn firmware tiếp theo sẽ được triển khai theo phạm vi và thứ tự đã thống nhất.
