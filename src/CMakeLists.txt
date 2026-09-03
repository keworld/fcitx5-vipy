add_library(vipy MODULE
        # Headers (Khai báo để CLion nhận diện project target)
        ${CMAKE_SOURCE_DIR}/include/vipy/utf8_helper.hpp
        ${CMAKE_SOURCE_DIR}/include/vipy/vipy_config.hpp
        ${CMAKE_SOURCE_DIR}/include/vipy/vipy-icon-resolver.hpp

        # Sources
        fcitx5/vipy-icon-resolver.cpp
        utf8/vietnamese_text.hpp
        utf8/vietnamese_text.cpp
        python/python_error.cpp
        python/python_runtime.cpp
        python/python_engine.cpp
        engine/vipy_state.cpp
        engine/mode_action.cpp
        engine/vipy_engine.cpp
        engine/vipy_addon_factory.cpp
)

# Định nghĩa Include Directories
target_include_directories(vipy PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}
)
target_compile_definitions(vipy PRIVATE
        FCITX_VIPY_ICON_DIR="${CMAKE_INSTALL_FULL_DATADIR}/icons/hicolor/scalable/apps"
        VIPY_PYTHON_MODULE_DIR="${VIPY_PYTHON_MODULE_DIR}"
)

# Link thư viện Fcitx5
target_link_libraries(vipy PRIVATE
        Fcitx5::Core
        Fcitx5::Utils
        Python3::Python
        ${CMAKE_DL_LIBS}
)

# Python loads extension modules (for example ``unicodedata``) while this
# addon is loaded as a plugin.  Export libpython's symbols from the plugin so
# those extensions can resolve symbols such as PyModule_Type.
target_link_options(vipy PRIVATE "-Wl,-z,global")

# Loại bỏ tiền tố 'lib' mặc định để sinh ra vipy.so thay vì libvipy.so
set_target_properties(vipy PROPERTIES PREFIX "")

# Cài đặt target module vào thư mục addon do Fcitx5 xác định.
# Biến này cũng xử lý đúng prefix tùy chỉnh và cấu hình multiarch của distro.
install(TARGETS vipy DESTINATION ${FCITX_INSTALL_ADDONDIR})