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
│   │   ├── probe_state/   # Snapshot trạng thái giữa các task
│   │   ├── probe_console/ # Lệnh chẩn đoán/provision qua UART
│   │   ├── probe_oled/    # SSD1306 I2C và renderer chữ
│   │   ├── probe_encoder/ # A/B, nút và decoder độc lập
│   │   └── probe_local_ui/ # Menu, trạng thái và yêu cầu đổi SWD clock
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
│   └── 09_integration_validation.md
├── LICENSE
└── README.md
```

## Build nhanh

Nếu bộ công cụ cục bộ đã được chuẩn bị:

```powershell
.\01_source\tools\idf.ps1 --version
.\01_source\tools\idf.ps1 build
```

Firmware đầu ra: `01_source/build/stm32_wifi_probe.bin`.
Hướng dẫn cho máy mới: [Build và bộ công cụ](02_document/02_build_and_tooling.md).

## Trạng thái

Firmware **0.1.0-dev1** đã được build và nạp lên ESP32-S3 N16R8 ở COM13.
Đã kiểm tra flash 16 MB, PSRAM 8 MB, kết nối Wi-Fi TRIET và GDB/TCP trên phần cứng thật.
Backend Black Magic Debug và SWD port đã tích hợp; thử quét khi chưa có target trả lỗi
có kiểm soát. OLED/encoder đã có driver và menu, hoạt động thiếu OLED không gây dừng probe.

Chưa nghiệm thu debug/nạp flash trên STM32 hoặc hiển thị/thao tác trên OLED/encoder thật
vì chưa đấu nối các phần cứng này. MCUViewer acquisition, SWO và USB/CMSIS-DAP ở các mốc sau.
Xem [kết quả kiểm tra và giới hạn](02_document/09_integration_validation.md).

## Kết nối và sử dụng

```powershell
.\01_source\tools\idf.ps1 -p COM13 -b 460800 flash monitor
```

UART console 115200 baud: `help`, `status`, `pins`, `clock 400000`.
Cấu hình STA bằng `wifi <SSID>|<password>`; firmware lưu vào NVS và tự kết nối sau reset.
Không ghi thông tin đăng nhập vào source hoặc file cấu hình Git. Lệnh `ap` hiển thị
thông tin AP dự phòng trên console khi cần, IP mặc định `192.168.4.1`.

Dùng IP từ `status` để kết nối ARM GDB:

```text
target extended-remote <probe-ip>:2022
monitor swd_scan
attach 1
```

Chỉ `attach 1` khi scan đã liệt kê target. Chưa có STM32 nối dây thì scan báo lỗi là
kết quả mong đợi. Hướng dẫn đấu nối nằm trong [tài liệu phần cứng](02_document/05_hardware_and_local_ui.md).

## Quản lý phiên bản

Repository: [nguyentriet0810/esp32-stm32-debug](https://github.com/nguyentriet0810/esp32-stm32-debug), chế độ **private**.
Git quản lý firmware, tài liệu, cấu hình mặc định, script và các phiên bản dependency.
ESP-IDF được quản lý bằng submodule; build, cache và binary toolchain được `.gitignore`
loại khỏi commit. Toàn bộ bộ công cụ vẫn nằm cục bộ dưới `01_source/.tooling`.

Xem [Tổng quan dự án](02_document/01_project_overview.md) và
[Quy trình Git](02_document/03_git_workflow.md).

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
