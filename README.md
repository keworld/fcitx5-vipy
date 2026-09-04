# fcitx5-vipy
Fcitx5 Vietnamese Input Method \
Bộ gõ tiếng Việt dành cho Fcitx5 trên Linux.

## 🛠 Tính năng chính
- Hỗ trợ kiểu gõ Telex/VNI cơ bản.
- Tích hợp trực tiếp với bộ gõ Fcitx5.

## 📋 Yêu cầu hệ thống
- Linux (Arch Linux, Ubuntu, Debian,...)
- Fcitx5 development libraries (`fcitx5`, `libfcitx5utils-dev`)
- C++20 compiler (GCC / Clang)
- Python 3 development headers/runtime (used by the Fcitx5 wrapper)
- CMake (>= 3.10)

## 👁️‍🗨️ Demo
![Demo](./assets/demo.GIF)

## 🚀 Hướng dẫn biên dịch và cài đặt
1. Cài đặt gói phụ thuộc:
   
**Arch Linux:**
```bash
sudo pacman -S fcitx5 cmake gcc
```
**Ubuntu/Debian:**
```bash
sudo apt install libfcitx5utils-dev cmake g++
```
2. Clone repository:
```bash
git clone https://github.com/keworld/fcitx5-vipy.git
cd fcitx5-vipy
```
3. Tạo thư mục build và biên dịch:
```bash
mkdir build && cd build
cmake ..
make
```
   Các script và dữ liệu mặc định được cài vào `/usr/share/fcitx5-vipy`.
   Ở lần chạy đầu tiên, Vipy tự copy các file mặc định vào
   `~/.config/fcitx5-vipy/` (hoặc `$XDG_CONFIG_HOME/fcitx5-vipy/`). Python
   script, từ điển và macro trong thư mục này được ưu tiên tuyệt đối, nên người
   dùng chỉ cần sửa file rồi restart Fcitx5; không cần biên dịch hoặc cài lại
   addon. File mặc định chỉ được copy khi file user chưa tồn tại.
   Nếu muốn tối ưu hóa cho local release build, có thể bật tùy chọn:
```bash
cmake .. -DVIPY_ENABLE_NATIVE_OPTIMIZATIONS=ON
make
```
4. Cài đặt vào hệ thống: 

Khi đóng gói cho Arch Linux, package nên phụ thuộc `python` lúc chạy và
`librsvg` lúc build (project dùng `rsvg-convert` để tạo status icon). Không
truyền `VIPY_PYTHON_MODULE_DIR` hoặc `VIPY_DATA_DIR` trỏ vào thư mục home của
máy build.bash
```bash
sudo make install
```
