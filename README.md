# ESP32 STM32 Debug

Công cụ debug STM32 sử dụng **ESP32-S3 N16R8** làm probe.
Nền tảng phát triển: **ESP-IDF v6.1** trên Windows x64.

## Cấu trúc dự án

```text
esp32-stm32-debug/
├── 01_source/
│   ├── .tooling/
│   │   ├── esp-idf-v6.1/   # Git submodule, ghim đúng commit v6.1
│   │   ├── idf-tools/      # Compiler, CMake, Ninja, GDB, OpenOCD...
│   │   ├── python/         # Python runtime cục bộ
│   │   └── python-env/     # Python environment và các dependency
│   ├── cmake/             # Quy chuẩn build cho mã dự án
│   ├── components/
│   │   ├── probe_board/   # Profile N16R8, kiểm tra bộ nhớ và pin
│   │   ├── probe_bmd/     # Adapter build Black Magic Debug
│   │   ├── probe_debug/   # GDB/TCP, SWD và platform port
│   │   ├── probe_network/ # AP/STA và cấu hình Wi-Fi trong NVS
│   │   ├── probe_wifi_setup/ # Web cấu hình, scan Wi-Fi và Ping gateway router
│   │   ├── probe_state/   # Snapshot trạng thái giữa các task
│   │   ├── probe_console/ # Lệnh chẩn đoán/provision qua UART
│   │   ├── probe_sd/      # microSD SPI/FAT, mount và đọc/ghi có giới hạn
│   │   ├── probe_oled/    # SH1106 I2C và renderer chữ
│   │   ├── probe_encoder/ # A/B, nút và decoder độc lập
│   │   └── probe_local_ui/ # Startup và Settings mẫu, encoder chọn/chỉnh giá trị RAM
│   ├── external/blackmagic/ # Git submodule, backend v2.1.0
│   ├── main/              # Entry point, ghép component và chính sách khởi động
│   ├── tests/             # Bài thử decoder trên PC
│   ├── tooling/           # Dependency lock và constraints được quản lý bằng Git
│   ├── tools/             # Script setup và build
│   ├── CMakeLists.txt
│   ├── partitions.csv
│   ├── sdkconfig.defaults
│   └── tooling.lock.json
├── 02_document/
│   ├── 01_project_overview.md
│   ├── 02_build_and_tooling.md
│   ├── 03_git_workflow.md
│   ├── 04_development_plan.md
│   ├── 05_hardware_and_local_ui.md
│   ├── 06_coding_standard.md
│   ├── 07_misra_deviations.md
│   ├── 08_third_party_notices.md
│   ├── 09_integration_validation.md
│   └── 10_project_logic.md
├── LICENSE
└── README.md
```

## Lập trình firmware ESP32 trong VS Code

Chạy `./open-vscode.ps1` từ thư mục repository để mở
[esp32-probe.code-workspace](esp32-probe.code-workspace) bằng profile VS Code riêng
**ESP32 Probe**. Profile chỉ có ESP-IDF, C/C++ và CMake syntax.
Workspace dùng ESP-IDF v6.1 cục bộ, cấu hình build Debug và console USB native tại COM9.
`Ctrl+Shift+B` để build; `F5` để build, nạp và debug ESP32-S3 qua USB-JTAG.
Task `ESP32: Monitor` mở console; `Ctrl+]` để đóng monitor trước khi nạp/debug.
Hướng dẫn chi tiết nằm trong [Build và bộ công cụ](02_document/02_build_and_tooling.md).

## Build nhanh

Nếu bộ công cụ cục bộ đã được chuẩn bị:

```powershell
.\01_source\tools\idf.ps1 --version
.\01_source\tools\idf.ps1 reconfigure build
```

Firmware đầu ra: `01_source/build/stm32_wifi_probe.bin`.
Hướng dẫn cho máy mới: [Build và bộ công cụ](02_document/02_build_and_tooling.md).
Firmware hiện dùng pin cố định theo schematic revision C của PCB REV 1.0:
SWDIO/SWCLK/SWO/nRST = GPIO4/5/6/7, OLED = GPIO8/9, encoder = GPIO15/16/17.
microSD dùng SPI tại GPIO38–42, mount FAT tại `/sdcard`; thiếu thẻ không dừng probe.
Flash N16 đã chia hai slot ESP32 4 MiB, NVS 64 KiB, PHY 8 KiB, otadata 8 KiB,
coredump 256 KiB, hai vùng ảnh STM32 `fw_1`/`fw_2` mỗi vùng 2 MiB và FAT `storage`
3,625 MiB. Nhãn `ota_1`/`ota_2` dùng subtype SDK ota_0/ota_1; app đầu ở `0x20000`.
[Bảng địa chỉ và cách chuyển NVS](02_document/02_build_and_tooling.md#cấu-hình-bộ-nhớ).
Dung lượng đã phân bổ; các luồng OTA, lưu ảnh STM32 và mount FAT nội bộ chưa triển khai.

## Trạng thái

Firmware **0.1.0-dev1** đã từng được build và nạp lên ESP32-S3 N16R8 DevKit ở COM13.
Đã kiểm tra flash 16 MB, PSRAM 8 MB, kết nối Wi-Fi TRIET và GDB/TCP trên phần cứng thật.
Backend Black Magic Debug và SWD port đã tích hợp; thử quét khi chưa có target trả lỗi
có kiểm soát. OLED/encoder đã có driver và menu, hoạt động thiếu OLED không gây dừng probe.

PCB COM9 đã có OLED SH1106, encoder và microSD; firmware báo OLED online, SD mounted.
UI đã có startup, Settings và các màn con SWD/Wi-Fi. Web nhập cấu hình bằng điện thoại
và scan Wi-Fi đã build/nạp, lưu profile và kết nối lại router thật sau reset thành công.
Ping tới hotspot Windows hiện không có phản hồi ICMP; kết nối TCP tới probe vẫn hoạt động.
Chưa nghiệm thu debug/nạp flash trên STM32, timing SWD và đầy đủ thao tác encoder thật.
MCUViewer acquisition, SWO và USB/CMSIS-DAP ở các mốc sau.
Xem [kết quả kiểm tra và giới hạn](02_document/09_integration_validation.md).

## Kết nối và sử dụng

```powershell
.\01_source\tools\idf.ps1 -p COM13 -b 460800 flash monitor
```

UART console 115200 baud: `help`, `status`, `pins`, `clock 400000`.
Thẻ SD: `sd`, `sd mount`, `sd ls`, `sd read TEST.TXT`,
`sd write TEST.TXT|Hello SD`, `sd unmount`. Tên file dạng 8.3 ở thư mục gốc;
lệnh ghi tạo file mới, không ghi đè. Xem [hướng dẫn microSD](02_document/11_sd_storage.md).
Cấu hình STA bằng `wifi <SSID>|<password>`; firmware lưu vào NVS và tự kết nối sau reset.
Không ghi thông tin đăng nhập vào source hoặc file cấu hình Git. Lệnh `ap` hiển thị
thông tin AP dự phòng trên console khi cần, IP mặc định `192.168.4.1`.

Trên OLED, chọn **Settings → Link → WiFi**, bật **STA setup ON**, nối điện thoại với
AP **MAGIC-PROBE**, mật khẩu **1234567890**, rồi mở **http://192.168.4.1**.
STA setup chỉ bật/tắt cấu hình web, không ngắt STA đang nối router. Khi chưa có STA,
OLED hiện `STA: NONE` và setup luôn ON; sau khi kết nối có thể chủ động chuyển OFF.
Mức **OLED %** được lưu NVS khi nhấn SW chốt và khôi phục sau reset; xem trước/Back không ghi.
50 % chỉ là giá trị dự phòng khi chưa có mức lưu hợp lệ.
Trang cấu hình dùng tiếng Anh: bấm **Scan Wi-Fi**, chọn mạng
trong **Available networks** để điền SSID, nhập mật khẩu router 2,4 GHz rồi bấm **Save and connect**;
cấu hình được lưu NVS, OLED hiện tên STA khi có IP. Ping router nằm trên trang web và
console; màn WiFi OLED có hàng trống dưới tiêu đề, ẩn cả IP/AP/PW khi setup OFF và giữ Back.
Console có `web on`, `web off`, `scan`, `ping` và `net` để cấu hình/chẩn đoán. Xem [hướng dẫn UI/Wi-Fi](02_document/05_hardware_and_local_ui.md).

Dùng IP từ `status` để kết nối ARM GDB:

```text
target extended-remote <probe-ip>:2022
monitor swd_scan
attach 1
```

Chỉ `attach 1` khi scan đã liệt kê target. Chưa có STM32 nối dây thì scan báo lỗi là
kết quả mong đợi. Hướng dẫn đấu nối nằm trong [tài liệu phần cứng](02_document/05_hardware_and_local_ui.md).

Giao diện debug được chọn là **VS Code + Cortex-Debug**, kết nối tới GDB/TCP của probe.
Xem [hướng dẫn và cấu hình attach mẫu](02_document/12_vscode_debug.md).
Mẫu dùng trong project ứng dụng STM32; phiên debug trên target thật vẫn chờ nghiệm thu.

## Quản lý phiên bản

Repository: [nguyentriet0810/esp32-stm32-debug](https://github.com/nguyentriet0810/esp32-stm32-debug), chế độ **public**.
Git quản lý firmware, tài liệu, cấu hình mặc định, script và các phiên bản dependency.
ESP-IDF được quản lý bằng submodule; build, cache và binary toolchain được `.gitignore`
loại khỏi commit. Toàn bộ bộ công cụ vẫn nằm cục bộ dưới `01_source/.tooling`.

Xem [Tổng quan dự án](02_document/01_project_overview.md) và
[Quy trình Git](02_document/03_git_workflow.md).

Kiến trúc, vòng đời phiên và luồng debug/nạp firmware theo từng khối:
[Logic hoạt động dự án](02_document/10_project_logic.md).

Kế hoạch các giai đoạn, đầu ra và tiêu chí nghiệm thu:
[Kế hoạch phát triển](02_document/04_development_plan.md).

Pin map và giao diện OLED/encoder:
[Phần cứng và giao diện tại probe](02_document/05_hardware_and_local_ui.md).

## Quy chuẩn mã nguồn và giấy phép

Mã C tự viết dùng C17; adapter cần header Xtensa/FreeRTOS dùng GNU17 với ngoại lệ được ghi rõ.
Component chia theo trách nhiệm, comment API theo Doxygen.
Hướng dẫn mục tiêu là MISRA C:2025; hiện có compiler warnings/GCC analyzer và hồ sơ
ngoại lệ nền tảng, chưa có kết luận tuân thủ MISRA đầy đủ.
Xem [Quy chuẩn viết mã](02_document/06_coding_standard.md) và
[Hồ sơ ngoại lệ](02_document/07_misra_deviations.md).

Giấy phép mã gốc dự án: **GPL-3.0-or-later**, theo lựa chọn giữ hướng Black Magic Debug.
Xem [LICENSE](LICENSE) và [thông báo dependency](02_document/08_third_party_notices.md).
