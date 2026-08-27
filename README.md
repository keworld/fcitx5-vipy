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
- gperf (dùng để sinh bảng tra cứu tối ưu tại thời điểm build)

## 👁️‍🗨️ Demo
![Demo](./assets/demo.GIF)

## 🚀 Hướng dẫn biên dịch và cài đặt
1. Cài đặt gói phụ thuộc:
   
**Arch Linux:**
```bash
sudo pacman -S fcitx5 gperf cmake gcc
```
**Ubuntu/Debian:**
```bash
sudo apt install libfcitx5utils-dev gperf cmake g++
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
4. Cài đặt vào hệ thống: 
```bash
sudo make install
