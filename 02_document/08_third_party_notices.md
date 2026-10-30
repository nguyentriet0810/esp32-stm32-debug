# Giấy phép và dependency

## Mã gốc của dự án

Copyright (C) 2026 Nguyen Hoang Minh Triet.
Mã nguồn và tài liệu gốc của dự án được cấp phép **GPL-3.0-or-later**.
Xem văn bản [GNU GPLv3](../LICENSE) và SPDX trên file mã nguồn để biết lựa chọn
phiên bản 3 hoặc phiên bản mới hơn. Repo được công khai theo lựa chọn của chủ dự án.

## ESP-IDF và bộ công cụ

- ESP-IDF nằm trong submodule đã ghim `01_source/.tooling/esp-idf-v6.1`.
- Giữ nguyên `LICENSE`, copyright và giấy phép riêng của từng component upstream.
  Không coi mọi file trong SDK hoặc toolchain đều mang giấy phép của dự án.
- Compiler, Python và các binary hỗ trợ giữ giấy phép/thông báo đi kèm bản phân phối;
  chúng có cục bộ trong `.tooling` và không được push như mã gốc của repository.
- Bản constraints Python được lưu từ bộ công cụ Espressif; giữ nguồn gốc của file này
  khi cập nhật. Danh sách package trong lock file là bản ghi môi trường, không thay giấy phép package.

## Backend và dự án tham khảo

- [Black Magic Debug](https://codeberg.org/blackmagic-debug/blackmagic) đã được tích hợp bằng
  submodule `01_source/external/blackmagic`, v2.1.0, commit
  `449730f8bc6f7a41e3d39e47bc03089b37d4656c`. Giữ giấy phép/copyright từng file upstream.
  CMake adapter lựa chọn core GDB/Cortex-M/ADI và nhóm driver STM32, không sửa checkout upstream.
- [MCUViewer-probe](https://github.com/klonyyy/MCUViewer-probe) là dự án tham khảo,
  firmware dùng GPL-3.0-or-later. Bản tham chiếu develop commit
  `02755f409634f0874fb087b720d8883ef6f65dd8`.
  `probe_debug/port/probe_swd.c`, `probe_platform.c` và GDB poll/transport được chuyển đổi
  từ mã tham khảo, giữ copyright Piotr Wasilewski và Black Sphere khi có nguồn gốc tương ứng.
  Các shim `probe_bmd/port/platform.h`, `libc_compat.h/.c` giữ thông báo gốc của bản tham chiếu.
- Một số file có thể có giấy phép riêng, ví dụ header giao thức. Kiểm tra theo file và
  giữ nguyên thông báo tương ứng, không đổi toàn bộ thành SPDX của dự án.

Văn bản `LICENSE` hiện được lấy nguyên bản từ `COPYING` của MCUViewer-probe,
là văn bản GNU GPLv3. Project không đổi giấy phép mã dependency thành giấy phép riêng.
