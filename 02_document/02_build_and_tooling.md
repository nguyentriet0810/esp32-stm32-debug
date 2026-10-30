# Build và bộ công cụ

## Bộ công cụ cục bộ

Mọi đường dẫn build được tính từ vị trí `01_source`, gồm:

- `.tooling/esp-idf-v6.1`: ESP-IDF và các submodule thành phần.
- `.tooling/idf-tools`: toolchain, công cụ hỗ trợ và bản tải về được cache.
- `.tooling/python`: Python runtime Windows x64.
- `.tooling/python-env`: môi trường Python cho ESP-IDF.

Không cần trỏ về `D:\ota_update` hoặc dùng Python tại `C:\Espressif` để build.
Script cập nhật đường dẫn nền của venv theo vị trí project khi chạy.
Git và Windows vẫn là các điều kiện của máy host.

`tooling.lock.json` ghim ESP-IDF v6.1 tại commit
`fff9895c82d744c7237be8847347bdd1b07c6643` và ghi phiên bản bộ công cụ tham chiếu.
Danh sách công cụ tải về và checksum nằm trong `tools/tools.json` của SDK đã ghim.
Dependency Python được lưu tại `01_source/tooling/python-requirements.lock.txt`.

## Build với bộ công cụ đã có

Từ thư mục gốc repository:

```powershell
.\01_source\tools\idf.ps1 --version
.\01_source\tools\idf.ps1 build
```

Có thể gọi script bằng đường dẫn tuyệt đối từ bất kỳ thư mục nào.
Tham số được chuyển trực tiếp cho `idf.py`, không ghép thành lệnh để đánh giá lại.
Output và cache nằm trong `01_source/build` và `01_source/.cache`.

Đã kiểm chứng build firmware bằng SDK, compiler và Python cục bộ trong `01_source`;
script setup cũng đã chạy thành công trên môi trường hiện có. Chưa kiểm thử quy trình
khôi phục toàn bộ trên một máy Windows sạch và chưa nạp board thật.

## Clone và khôi phục trên máy mới

```powershell
git clone --recurse-submodules https://github.com/nguyentriet0810/esp32-stm32-debug.git
cd esp32-stm32-debug
.\01_source\tools\setup.ps1 -PythonHome 'C:\Path\To\Python311'
.\01_source\tools\idf.ps1 build
```

Vì repo private, Git cần được đăng nhập bằng tài khoản được cấp quyền.
`-PythonHome` trỏ tới bộ cài Python 3.11 Windows x64 có `python.exe` và thư viện chuẩn;
script sao chép runtime vào `.tooling/python`. Bản tham chiếu hiện dùng Python 3.11.2.
Nếu đã khôi phục `.tooling/python`, có thể bỏ tham số này.

`setup.ps1` khôi phục SDK đúng gitlink, cài công cụ cho ESP32-S3 theo manifest của SDK,
tạo venv, cài dependency Python đã ghim và kiểm tra dependency của ESP-IDF.
Lần setup trên máy mới cần kết nối mạng. Binary toolchain không nằm trong lịch sử Git.

## Nạp và xem log

Sau khi xác định đúng cổng, thay `COM5` bằng cổng thực tế:

```powershell
.\01_source\tools\idf.ps1 -p COM5 flash monitor
```

Console chính là UART0; log ứng dụng xuất thêm qua USB Serial/JTAG của S3.
Nếu board có hai cổng USB, cổng sử dụng phụ thuộc mạch nối của model board đó.

Log ứng dụng mong đợi:

```text
Flash: physical 16777216 bytes, configured 16777216 bytes
PSRAM: 8388608 bytes
Board bring-up passed: ESP32-S3 N16R8
```

## Cấu hình bộ nhớ

Flash Quad SPI và PSRAM Octal SPI là hai cấu hình riêng. Không chọn Quad PSRAM cho R8.
`partitions.csv` hiện dành 4 MB cho ứng dụng, phần flash còn lại chưa sử dụng.
Chưa có chức năng cập nhật OTA.

`sdkconfig` là trạng thái sinh ra cục bộ. Git lưu `sdkconfig.defaults`.
Khi thay defaults sau lần build đầu, cập nhật bằng `menuconfig`, hoặc sao lưu
`sdkconfig` trước khi tạo lại; giá trị đã có trong `sdkconfig` được ưu tiên hơn defaults.
