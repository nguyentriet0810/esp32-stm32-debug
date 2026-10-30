# Quy trình Git

## Repository

- URL: https://github.com/nguyentriet0810/esp32-stm32-debug
- Quyền truy cập: private.
- Nhánh chính: `main`.
- Root Git: thư mục gốc chứa `01_source`, `02_document` và `README.md`.

## Nội dung được quản lý

| Nội dung | Cách quản lý |
|---|---|
| Firmware và cấu hình mặc định | Commit trực tiếp |
| Tài liệu và script | Commit trực tiếp |
| ESP-IDF | Submodule tại commit cụ thể |
| Phiên bản công cụ và package Python | Lock file, constraints và manifest của SDK |
| Compiler, GDB, CMake, Ninja, Python và venv | Có cục bộ trong `.tooling`, không push binary |
| Build, cache, sdkconfig sinh ra, log | `.gitignore` |

Không commit Wi-Fi password, access token hoặc cấu hình riêng của máy.
`dependencies.lock` của ESP-IDF component manager cần được commit khi dự án có
dependency do component manager quản lý.

## Một thay đổi thông thường

```powershell
git switch -c feature/swd-bringup
# Sửa mã nguồn và tài liệu liên quan.
.\01_source\tools\idf.ps1 build
git status --short
git diff --check
git diff
git add 01_source/main 02_document
git diff --cached --stat
git diff --cached
git commit -m "feat: add basic SWD target identification"
git push -u origin feature/swd-bringup
```

Chỉ stage các đường dẫn của thay đổi đang làm. Kiểm tra cả nội dung staged trước khi push.
Mã C mới tuân theo [quy chuẩn dự án](06_coding_standard.md), có SPDX GPL-3.0-or-later,
comment API và kết quả build/analyzer. Ngoại lệ mới được ghi trong hồ sơ tương ứng.

## Đồng bộ submodule

```powershell
git pull --ff-only
git submodule update --init --recursive
```

Khi nâng SDK, tạo nhánh riêng, đổi gitlink và `tooling.lock.json`, cập nhật Python lock
nếu cần, build và kiểm tra trên board trước khi merge. Không dùng `git submodule update --remote`
cho build thông thường vì lệnh đó có thể chuyển SDK khỏi commit đã chốt.
