# Giấy phép và dependency

## Mã gốc của dự án

Copyright (C) 2026 Nguyen Hoang Minh Triet.
Mã nguồn và tài liệu gốc của dự án được cấp phép **GPL-3.0-or-later**.
Xem văn bản [GNU GPLv3](../LICENSE) và SPDX trên file mã nguồn để biết lựa chọn
phiên bản 3 hoặc phiên bản mới hơn. Repo vẫn có quyền truy cập private như đã chốt.

## ESP-IDF và bộ công cụ

- ESP-IDF nằm trong submodule đã ghim `01_source/.tooling/esp-idf-v6.1`.
- Giữ nguyên `LICENSE`, copyright và giấy phép riêng của từng component upstream.
  Không coi mọi file trong SDK hoặc toolchain đều mang giấy phép của dự án.
- Compiler, Python và các binary hỗ trợ giữ giấy phép/thông báo đi kèm bản phân phối;
  chúng có cục bộ trong `.tooling` và không được push như mã gốc của repository.
- Bản constraints Python được lưu từ bộ công cụ Espressif; giữ nguồn gốc của file này
  khi cập nhật. Danh sách package trong lock file là bản ghi môi trường, không thay giấy phép package.

## Backend và dự án tham khảo

- [Black Magic Debug](https://black-magic.org/docs_v1/hacking/hacking/) là backend dự kiến,
  firmware upstream dùng GPLv3. Chưa đưa mã backend vào repository này.
- [MCUViewer-probe](https://github.com/klonyyy/MCUViewer-probe) là dự án tham khảo,
  firmware dùng GPL-3.0-or-later. Khi dùng mã port cần giữ thông tin tác giả/giấy phép từng file.
- Một số file có thể có giấy phép riêng, ví dụ header giao thức. Kiểm tra theo file và
  giữ nguyên thông báo tương ứng, không đổi toàn bộ thành SPDX của dự án.

Văn bản `LICENSE` hiện được lấy nguyên bản từ `COPYING` của MCUViewer-probe,
là văn bản GNU GPLv3. Việc thêm văn bản giấy phép không có nghĩa đã tích hợp firmware của repo mẫu.
