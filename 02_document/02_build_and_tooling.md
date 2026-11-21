# Build và bộ công cụ

## Phát triển firmware ESP32 trong VS Code

Từ thư mục repository, chạy launcher để luôn chọn profile VS Code riêng của dự án:

```powershell
.\open-vscode.ps1
# Hoặc gọi trực tiếp:
code --new-window --profile "ESP32 Probe" .\esp32-probe.code-workspace
```

Profile **ESP32 Probe** được tạo từ profile trống, có thiết lập và danh sách extension
riêng. Workspace đã được gắn với profile này trên máy hiện tại. Chỉ có ba extension:

- `espressif.esp-idf-extension` **2.3.0**: build, nạp và debug ESP32.
- `ms-vscode.cpptools` **1.35.6**: IntelliSense và định dạng C/C++.
- `twxs.cmake` **0.0.17**: tô màu cú pháp CMake.

STM32Cube, PlatformIO, Cortex-Debug và CMake Tools không được cài trong profile này.
Các profile khác giữ nguyên danh sách extension và thiết lập. Trên máy mới, lệnh mở
ở trên tạo profile trống nếu chưa có; sau đó cài ba extension được workspace đề xuất
vào **ESP32 Probe**. Launcher không tự cài extension.

Workspace chỉ mở `01_source`. SDK, Python, compiler, GDB, OpenOCD và clang-format đều trỏ tới
bộ công cụ cục bộ đã ghim. Không cần chạy Installation Manager để tải một SDK khác.
`idf.extensionActivationMode = always` kích hoạt ESP-IDF cho workspace này dù bộ
nhận diện tự động không nhận CMakeLists có kiểm tra phiên bản SDK của dự án.
Launcher sinh `01_source/.vscode/settings.json` với đường dẫn môi trường thực tế từ
template `idf.customExtraVars` trong workspace; ESP-IDF 2.3 không thay biến
`${workspaceFolder}` bên trong object này. File cục bộ được Git bỏ qua; chạy lại
launcher sau khi chuyển repository sang đường dẫn khác.

Cấu hình build Debug dùng `sdkconfig.vscode.defaults` sau `sdkconfig.defaults`, sinh
`sdkconfig.vscode` và output trong `build-vscode`. Optimization Debug (`-Og`) giúp xem
biến và step code; GPIO/N16R8 giữ theo PCB. Console chính qua USB Serial/JTAG nên các
lệnh `status`, `pins`, `wifi`, `sd` dùng trực tiếp trên COM9. Build CLI `tools/idf.ps1`
và cấu hình `sdkconfig` hiện có vẫn độc lập với profile này.

| Thao tác | Cách dùng |
|---|---|
| Build Debug | `Ctrl+Shift+B` hoặc task `ESP32: Build` |
| Nạp firmware | Task `ESP32: Flash` hoặc `ESP32: Build + Flash` |
| Xem log và gửi lệnh USB | Task `ESP32: Monitor`; thoát bằng `Ctrl+]` |
| Build, nạp và mở console | Task `ESP32: Build + Flash + Monitor` |
| Debug ESP32-S3 qua USB-JTAG | `F5`, chọn `ESP32-S3: Build, Flash and Debug (USB-JTAG)` |
| Chỉnh cấu hình | Task `ESP32: Menuconfig` |
| Sinh lại build database | Task `ESP32: Reconfigure` |

`F5` build và nạp bản Debug trước, rồi kết nối OpenOCD và dừng ở `app_main`.
OpenOCD thực hiện `init; reset halt` trước khi GDB kết nối để đọc bản đồ flash từ
trạng thái dừng xác định; server chỉ lắng nghe trên localhost.
Đặt breakpoint bằng `F9`; step `F10`/`F11`, chạy tiếp `F5`, dừng debug `Shift+F5`.
Đây là debug **ESP32 của probe**, không phải kết nối tới STM32 qua TCP2022.
Đóng monitor trước khi Flash hoặc Debug để giải phóng cổng COM.
Nếu Windows đổi số COM, sửa `idf.port` trong file workspace cho đúng thiết bị.

Mở terminal mới trong workspace sẽ có sẵn ESP-IDF và lệnh `idf.py` của profile lập trình:

```powershell
idf.py build
idf.py -p COM9 flash
idf.py -p COM9 monitor --no-reset
idf.py menuconfig
```

Trong terminal bên ngoài VS Code có thể gọi tương đương:

```powershell
.\01_source\tools\dev.ps1 build
.\01_source\tools\dev.ps1 -p COM9 flash
```

IntelliSense đọc `build-vscode/compile_commands.json`; build/reconfigure lần đầu cần
hoàn tất trước khi editor có đủ thông tin header. Formatter dùng `.clang-format` của
repository. `.tooling` và `.cache` được ẩn trong Explorer để tập trung vào mã dự án.

Các file cấu hình workflow được giữ trong workspace, không ghi đè thiết lập VS Code
toàn cục hoặc các project STM32 khác. Tài liệu attach STM32 riêng nằm ở
[hướng dẫn Cortex-Debug](12_vscode_debug.md).

Đã kiểm tra ngày 21/11/2026 trên PCB COM9: build và nạp profile Debug thành công;
USB-JTAG nhận đủ hai lõi, GDB dừng đúng `app_main`, step qua kiểm tra board và đọc
`result = 0`. Console USB trả lời `status` và `pins`; VS Code nhận cấu hình debug,
IntelliSense sẵn sàng và không báo Problems ở `main.c`. Log kiểm tra cục bộ nằm tại
`01_source/.cache/vscode-setup-20261121/`. Phép kiểm tra này không sửa lỗi remount SD.
Profile `ESP32 Probe` đã được kiểm tra có đúng ba extension trên, workspace mở đúng
profile và status bar nhận **ESP-IDF v6.1.0**. Danh sách 62 extension và checksum
settings của profile Default giữ nguyên; bằng chứng cục bộ nằm tại
`01_source/.cache/vscode-profile-20261121/`.

Tham chiếu cấu hình: [Profile và extension riêng trong VS Code](https://code.visualstudio.com/docs/configure/profiles),
[SDK cục bộ trong extension Espressif](https://docs.espressif.com/projects/vscode-esp-idf-extension/en/latest/installation.html)
và [debug USB-JTAG với gdbtarget](https://docs.espressif.com/projects/vscode-esp-idf-extension/en/latest/debugproject.html).

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
khôi phục toàn bộ trên một máy Windows sạch. Firmware đã nạp và chạy trên COM13;
xem [báo cáo kiểm tra](09_integration_validation.md).

Mã tự viết dùng C17, adapter có header Xtensa/FreeRTOS dùng GNU17 (SDK-004),
đều có strict warnings và GCC analyzer qua `cmake/project_options.cmake`.
Do SDK truyền thêm `-std=gnu23`, dialect dự án được đặt ở source option cuối cùng để có hiệu lực.
SDK và BMD giữ mode upstream; xem [quy chuẩn mã](06_coding_standard.md).

## Clone và khôi phục trên máy mới

```powershell
git clone --recurse-submodules https://github.com/nguyentriet0810/esp32-stm32-debug.git
cd esp32-stm32-debug
.\01_source\tools\setup.ps1 -PythonHome 'C:\Path\To\Python311'
.\01_source\tools\idf.ps1 build
```

Repo public, có thể clone qua HTTPS mà không cần đăng nhập GitHub.
`-PythonHome` trỏ tới bộ cài Python 3.11 Windows x64 có `python.exe` và thư viện chuẩn;
script sao chép runtime vào `.tooling/python`. Bản tham chiếu hiện dùng Python 3.11.2.
Nếu đã khôi phục `.tooling/python`, có thể bỏ tham số này.

`setup.ps1` khôi phục SDK và Black Magic Debug đúng gitlink/lock, cài công cụ cho ESP32-S3 theo manifest của SDK,
tạo venv, cài dependency Python đã ghim và kiểm tra dependency của ESP-IDF.
Lần setup trên máy mới cần kết nối mạng. Binary toolchain không nằm trong lịch sử Git.

## Nạp và xem log

Chọn cổng serial của thiết bị thực tế. `COM13` qua CH343 là thiết lập DevKit đã
kiểm tra trước đây; PCB revision C dùng UART0 tại J2 và USB native tại J1:

```powershell
.\01_source\tools\idf.ps1 -p COM13 -b 460800 flash monitor
```

Console chính là UART0 tại J2 (GPIO43 TX/GPIO44 RX), cần USB-UART ngoài mức 3,3 V;
J2 còn có DTR/RTS cho auto-reset. J1 nối USB native GPIO19/20 của S3, dùng nạp qua
USB Serial/JTAG và xuất log phụ. Firmware hiện đọc lệnh console từ UART0.

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

## Cấu hình và kiểm tra firmware

`menuconfig` → `STM32 probe hardware` cho phép đổi clock SWD khởi tạo, cổng TCP,
bật/tắt OLED và encoder. Pin cố định theo PCB mới nhất, không còn profile pin DevKit.
Profile N16R8 từ chối chân trùng hoặc chân dành cho flash/PSRAM,
USB, UART0, boot strap; nhóm microSD GPIO38–42 thuộc riêng component `probe_sd`. Defaults giữ OLED/encoder bật
để phát hiện khi cắm vào; thiếu OLED là lỗi không nghiêm trọng.

Pin mặc định theo schematic revision C: SWDIO/SWCLK/SWO/nRST = 4/5/6/7,
OLED SDA/SCL = 8/9, encoder A/B/SW = 15/16/17. Các symbol GPIO trong Kconfig không
còn cho sửa bằng menuconfig; khi reconfigure, giá trị pin đã lưu được thay bằng pin PCB.
Sau cập nhật source/Kconfig, chạy:

```powershell
.\01_source\tools\idf.ps1 reconfigure build
```

microSD được bật mặc định (`CONFIG_PROBE_SD_ENABLE=y`), SPI tối đa 4000 kHz qua
`CONFIG_PROBE_SD_MAX_FREQ_KHZ`. Có thể tắt riêng driver trong menuconfig; GPIO vẫn cố định.
Thẻ FAT được mount tại `/sdcard`; thiếu/lỗi thẻ không dừng probe, không tự format.
Xem [lệnh console và quy trình kiểm tra SD](11_sd_storage.md).
Kiểm tra cấu hình với netlist mới xuất từ schematic:

```powershell
New-Item -ItemType Directory -Force .\01_source\.cache\schematic_audit > $null
& 'C:\Program Files\KiCad\10.0\bin\kicad-cli.exe' sch export netlist --format kicadxml -o .\01_source\.cache\schematic_audit\latest.xml .\03_pcb\AI_layout\AI_layout.kicad_sch
& .\01_source\.tooling\python\python.exe .\01_source\tools\test_board_config.py --netlist .\01_source\.cache\schematic_audit\latest.xml --sdkconfig .\01_source\sdkconfig
```

Script kiểm tra 14 GPIO, kết nối J3/J4/J5 và pin trong cấu hình build.
Chú thích tự do trong schematic còn ghi nhóm SD GPIO14–18 là thông tin cũ; kết nối
netlist thực tế dùng CS/MOSI/SCK/MISO/DETECT = 38/39/40/41/42.

Console UART 115200: `wifi <SSID>|<password>`, `status`, `pins`, `clock <Hz>`, `ap`.
SSID 1–32 byte, mật khẩu WPA2 8–63 byte; dấu `|` phân cách nên không dùng trong SSID.
Không echo đầu vào credential, không đưa mật khẩu STA vào log. NVS hiện chưa mã hóa;
đây là cấu hình lưu cục bộ của nguyên mẫu. Lệnh `ap` chỉ in credential AP khi được gọi trực tiếp.
Đổi mạng bị từ chối khi có phiên GDB. Clock UI/console chỉ lưu RAM, trở lại defaults sau reset.

Thử transport trên probe đang chạy:

```powershell
. .\01_source\tools\environment.ps1
& $probePython .\01_source\tools\test_gdb_transport.py <probe-ip> --expect-no-target
```

Chỉ dùng `--expect-no-target` khi chưa nối STM32. Script thực hiện scan SWD nhưng không
erase/program flash. Test cần PC truy cập được IP của probe; script không tự đổi Wi-Fi PC.
