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
│   │   └── probe_board/   # Kiểm tra phần cứng N16R8
│   ├── main/              # Entry point, ghép component và chính sách khởi động
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
│   └── 08_third_party_notices.md
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

Hiện là project nền kiểm tra ESP32-S3 N16R8: flash 16 MB, PSRAM Octal 8 MB / 80 MHz,
CPU 240 MHz, hai lõi. SWD, Wi-Fi, GDB server và trace sẽ được phát triển theo từng mốc.
Thiết kế bổ sung OLED I2C SSD1306 128×64 và rotary encoder cho giao diện cục bộ;
driver và menu chưa được triển khai.
Chưa xác nhận runtime trên phần cứng thật.

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

Mã C tự viết dùng C17, chia component theo trách nhiệm và comment API theo Doxygen.
Hướng dẫn mục tiêu là MISRA C:2025; hiện có compiler warnings/GCC analyzer và hồ sơ
ngoại lệ nền tảng, chưa có kết luận tuân thủ MISRA đầy đủ.
Xem [Quy chuẩn viết mã](02_document/06_coding_standard.md) và
[Hồ sơ ngoại lệ](02_document/07_misra_deviations.md).

Giấy phép mã gốc dự án: **GPL-3.0-or-later**, theo lựa chọn giữ hướng Black Magic Debug.
Xem [LICENSE](LICENSE) và [thông báo dependency](02_document/08_third_party_notices.md).
