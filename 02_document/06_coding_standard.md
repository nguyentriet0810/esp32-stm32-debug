# Quy chuẩn tổ chức và viết mã

## 1. Phạm vi và mức kiểm chứng

- Mã C do dự án viết trong `01_source/main` và `01_source/components` dùng **C17**.
- Mục tiêu hướng dẫn là **MISRA C:2025**; quy trình đối chiếu theo MISRA Compliance:2020.
- Hiện áp dụng quy tắc dự án, compiler warnings, GCC static analyzer và review mã.
  Chưa có checker MISRA đầy đủ, guideline enforcement plan theo từng rule hoặc kết luận tuân thủ toàn bộ.
- Doxygen là quy ước comment của dự án, không phải một định dạng comment do MISRA bắt buộc.
- ESP-IDF và thư viện upstream được đánh giá riêng với mã tự viết; giữ nguyên giấy phép
  và cấu hình build cần thiết của chúng. ESP-IDF v6.1 mặc định GNU23; C17 được áp dụng riêng cho target dự án.

## 2. Ranh giới component

```text
main/                         # Entry point và chính sách khởi động
components/
  probe_board/
    include/probe_board.h     # API công khai
    probe_board.c             # Kiểm tra phần cứng N16R8 qua ESP-IDF
    CMakeLists.txt            # Dependency và cờ compiler của component
cmake/project_options.cmake   # C17, cảnh báo và GCC analyzer cho mã dự án
```

- `main` ghép các component, chọn hành vi khi khởi động thất bại và khởi chạy dịch vụ.
- Component có trách nhiệm cụ thể, API nhỏ và tài nguyên do một nơi sở hữu.
- Chỉ public header trong `include`; header nội bộ không được component khác truy cập.
- Public dependency khai báo `REQUIRES`, dependency chỉ dùng nội bộ khai báo `PRIV_REQUIRES`.
- Hàm/biến không nằm trong API dùng `static`; tránh dependency vòng và global có thể ghi từ nhiều module.
- Tạo `local_ui`, SWD port, network và probe service khi triển khai chức năng thực tế;
  không tạo module rỗng hoặc gom mọi chức năng vào một component tiện ích chung.

## 3. Quy tắc C của dự án

1. Khai báo prototype trước khi định nghĩa API; mỗi API có một nơi khai báo chính thức.
2. Dùng `stdint.h` cho giá trị phần cứng/giao thức, `size_t` cho kích thước bộ nhớ và `bool` cho điều kiện.
3. Hằng số ghi rõ đơn vị và kiểu. Phân biệt byte, MiB, Hz, ms và số tick.
4. Khởi tạo biến trước khi đọc; kiểm tra pointer, kích thước, alignment và giới hạn tại ranh giới API.
5. Chuyển kiểu phải có lý do và bảo đảm miền giá trị; không dùng cast để che warning.
6. Dùng toán hạng cùng miền kiểu, tránh phụ thuộc thứ tự đánh giá và tránh side effect trong điều kiện phức tạp.
7. Mọi nhánh điều khiển dùng dấu ngoặc khối; ưu tiên hàm nhỏ với luồng xử lý lỗi rõ ràng.
8. Không dùng recursion, VLA hoặc `goto` trong mã mới theo quy tắc dự án.
9. Không cấp phát/giải phóng động trong ISR hoặc đường timing SWD. Buffer dài hạn cấp phát ở
   giai đoạn thiết lập dịch vụ, kiểm tra lỗi và ghi rõ chủ sở hữu/vòng đời.
10. Driver/component trả mã lỗi; chính sách reset/abort thuộc tầng application.
11. I/O và giao dịch target có timeout; vòng chờ phần cứng phải có điều kiện thoát hữu hạn.
12. Đồng bộ task/ISR bằng queue, notification, atomic hoặc primitive phù hợp;
    `volatile` không thay thế đồng bộ giữa các task.
13. Một task sở hữu SWD. UI và network gửi yêu cầu qua dịch vụ, không thao tác target trực tiếp.
14. Macro chỉ dùng cho cấu hình/compile-time cần thiết; ưu tiên hàm cho logic và hằng có kiểu cho dữ liệu.
15. Phụ thuộc đặc tính compiler/SDK được ghi rõ trong [hồ sơ ngoại lệ](07_misra_deviations.md).

## 4. Đặt tên và định dạng

- Tên file/hàm/biến: `snake_case`; API có tiền tố module, ví dụ `probe_board_check`.
- Type riêng có hậu tố `_t`; enum/macro có tiền tố module và dùng chữ hoa.
- Hằng nội bộ có kiểu dùng `static const`, tên nêu ý nghĩa/đơn vị.
- Indent 4 dấu cách, ngoặc khối Allman, mục tiêu tối đa 100 ký tự/dòng.
- Include public header của module trước, sau đó thư viện C chuẩn và header dependency.
- `.editorconfig` và `.clang-format` là cấu hình định dạng. Máy hiện tại chưa có
  `clang-format`; không coi việc có file cấu hình là đã chạy kiểm tra formatter.
- Chỉ format mã dự án; không quét/sửa toàn bộ SDK hoặc dependency upstream.

## 5. Comment và tài liệu API

Mã nguồn dùng comment tiếng Anh; tài liệu thiết kế/hướng dẫn dùng tiếng Việt.
Mỗi file C/header có SPDX, `@file` và mô tả vai trò ngắn.
API công khai có `@brief`, mô tả kết quả/lỗi và các thông tin liên quan:

- `@param[in]`, `@param[out]`, `@param[in,out]` khi có tham số, gồm kích thước/đơn vị và quyền sở hữu.
- `@return`/`@retval`: các giá trị trả về và cách xử lý lỗi.
- `@pre`: điều kiện cần có trước khi gọi.
- `@note`: task/ISR context, khả năng gọi đồng thời, side effect hoặc vòng đời dữ liệu.

Comment trong thân hàm giải thích lý do, giả định phần cứng hoặc quyết định khó suy ra từ mã.
Không gán nhãn "MISRA compliant" chỉ bằng comment, không thêm rule ID chưa được đối chiếu.
Comment ngoại lệ dùng mã hồ sơ, ví dụ `SDK-001`, để tra lại phạm vi và lý do.

## 6. Kiểm tra bắt buộc khi thay đổi mã C

- Build bằng SDK/toolchain cục bộ đã ghim.
- Target mã dự án bật `-Wall`, `-Wextra`, `-Werror`, kiểm tra format/prototype/shadow
  và conversion/sign conversion trong `project_options.cmake`.
- GCC analyzer bật mặc định cho target mã C dự án qua `PROBE_ENABLE_GCC_ANALYZER`.
  Đây là phân tích tổng quát của compiler, không phải checker MISRA.
- Kiểm tra `compile_commands.json` để xác nhận mode C17/cờ kiểm tra có hiệu lực sau khi đổi build system.
- Review API, đường lỗi, tài nguyên, đồng bộ và ngoại lệ mới; thêm bài thử có ý nghĩa cho logic mới.
- Chạy bài thử phần cứng liên quan trước khi đánh dấu đã nghiệm thu chức năng.
- Checker MISRA chuyên dụng sẽ được cấu hình theo phiên bản tiêu chuẩn và bản tài liệu được cấp phép
  khi có công cụ. Các kết quả, ngoại lệ và guideline coverage cần được ghi lại trước khi tuyên bố tuân thủ.

## 7. Giấy phép

Mã gốc của dự án dùng **GPL-3.0-or-later** theo lựa chọn đã xác nhận của người dùng,
phù hợp với hướng tích hợp Black Magic Debug. Dùng SPDX `GPL-3.0-or-later` cho file mã mới.
Văn bản GPLv3 nằm trong `LICENSE` ở root; thông tin áp dụng phiên bản 3 hoặc mới hơn
nằm trong SPDX và tài liệu này. Không thay giấy phép/tác giả của dependency bằng giấy phép dự án.

## 8. Tham chiếu

- [MISRA Compliance:2020](https://www.misra.org.uk/app/uploads/2021/06/MISRA-Compliance-2020.pdf).
- [MISRA C:2025](https://misra.org.uk/product/misra-c2025/).
- [Giấy phép và dependency](08_third_party_notices.md).
