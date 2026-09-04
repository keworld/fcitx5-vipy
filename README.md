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
   Khi cài đặt vào hệ thống bằng `sudo`, hãy giữ nguyên người dùng gốc để file `~/.config/fcitx5-vipy` vẫn thuộc quyền của user thường:
```bash
sudo -E make install
```
   Nếu không dùng `sudo -E`, CMake sẽ tự phát hiện `SUDO_USER` và chown lại các file config về quyền người dùng thực tế sau khi install.
   Nếu muốn tối ưu hóa cho local release build, có thể bật tùy chọn:
```bash
cmake .. -DVIPY_ENABLE_NATIVE_OPTIMIZATIONS=ON
make
```
4. Cài đặt vào hệ thống: 
```bash
sudo make install
