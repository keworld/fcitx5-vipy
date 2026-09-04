cmake_minimum_required(VERSION 3.16)
project(fcitx5-vipy VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
enable_testing()

# Fcitx5 normally installs system addons below /usr, so use that as the
# default while still allowing packagers to override the prefix.
if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set(CMAKE_INSTALL_PREFIX "/usr" CACHE PATH "Default install prefix" FORCE)
endif()

# Cấu hình tối ưu hóa biên dịch theo hướng build ổn định và portable.
# -march=native / -flto được bật tùy chọn để dễ publish trên máy local mà không
# ép buộc mọi distro/CI đều phải build theo CPU target cụ thể.
option(VIPY_ENABLE_NATIVE_OPTIMIZATIONS "Enable native CPU tuning and LTO for local release builds" OFF)

add_compile_options(-O3 -fvisibility=hidden -fvisibility-inlines-hidden)
if(VIPY_ENABLE_NATIVE_OPTIMIZATIONS)
    add_compile_options(-march=native -flto)
    add_link_options(-flto)
endif()

include(GNUInstallDirs)

# Tìm kiếm các phụ thuộc bắt buộc
find_package(Fcitx5Utils REQUIRED)
find_package(Fcitx5Core REQUIRED)
find_package(Python3 REQUIRED COMPONENTS Interpreter Development.Embed)

if(NOT DEFINED ENV{HOME} OR "$ENV{HOME}" STREQUAL "")
    message(FATAL_ERROR "The HOME environment variable is required to install Vipy Python modules")
endif()

set(VIPY_PYTHON_MODULE_DIR
        "$ENV{HOME}/.config/fcitx5-vipy/script"
        CACHE PATH "User directory for Vipy Python modules")
set(VIPY_DATA_DIR
        "$ENV{HOME}/.config/fcitx5-vipy/data"
        CACHE PATH "User directory for Vipy data files")

file(GLOB VIPY_PYTHON_MODULES CONFIGURE_DEPENDS
        "${CMAKE_SOURCE_DIR}/scripts/*.py")

install(FILES ${VIPY_PYTHON_MODULES}
        DESTINATION ${VIPY_PYTHON_MODULE_DIR})

# Load thư mục con
add_subdirectory(src)
add_subdirectory(data)
add_subdirectory(tests)
