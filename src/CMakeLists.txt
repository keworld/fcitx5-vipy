add_library(vipy MODULE
        # Headers (Khai báo để CLion nhận diện project target)
        ${CMAKE_SOURCE_DIR}/include/vipy/utf8_helper.hpp
        ${CMAKE_SOURCE_DIR}/include/vipy/vipy_config.hpp
        ${CMAKE_SOURCE_DIR}/include/vipy/vipy-icon-resolver.hpp

        # Sources
        fcitx5/vipy-icon-resolver.cpp
        fcitx5/vipy_addon.cpp
)

# Định nghĩa Include Directories
target_include_directories(vipy PRIVATE
        ${CMAKE_SOURCE_DIR}/include
)
target_compile_definitions(vipy PRIVATE
        FCITX_VIPY_ICON_DIR="${CMAKE_INSTALL_FULL_DATADIR}/icons/hicolor/scalable/apps"
        VIPY_PYTHON_MODULE_DIR="${VIPY_PYTHON_MODULE_DIR}"
)

# Link thư viện Fcitx5
target_link_libraries(vipy PRIVATE Fcitx5::Core Python3::Python ${CMAKE_DL_LIBS})

# Python loads extension modules (for example ``unicodedata``) while this
# addon is loaded as a plugin.  Export libpython's symbols from the plugin so
# those extensions can resolve symbols such as PyModule_Type.
target_link_options(vipy PRIVATE "-Wl,-z,global")

# Loại bỏ tiền tố 'lib' mặc định để sinh ra vipy.so thay vì libvipy.so
set_target_properties(vipy PROPERTIES PREFIX "")

# Cài đặt target module vào thư mục addon do Fcitx5 xác định.
# Biến này cũng xử lý đúng prefix tùy chỉnh và cấu hình multiarch của distro.
install(TARGETS vipy DESTINATION ${FCITX_INSTALL_ADDONDIR})