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

# Cấu hình tối ưu hóa biên dịch
add_compile_options(-O3 -march=native -flto -fvisibility=hidden -fvisibility-inlines-hidden)

include(GNUInstallDirs)

# Tìm kiếm các phụ thuộc bắt buộc
find_package(Fcitx5Utils REQUIRED)
find_package(Fcitx5Core REQUIRED)
find_program(GPERF_EXECUTABLE gperf REQUIRED)
find_package(Python3 REQUIRED COMPONENTS Interpreter Development.Embed)

file(GLOB VIPY_PYTHON_MODULES CONFIGURE_DEPENDS
        "${CMAKE_SOURCE_DIR}/scripts/*.py")

install(FILES ${VIPY_PYTHON_MODULES}
        DESTINATION ${CMAKE_INSTALL_DATADIR}/fcitx5/vipy/scripts)

# Load thư mục con
add_subdirectory(src)
add_subdirectory(data)
add_subdirectory(tests)
