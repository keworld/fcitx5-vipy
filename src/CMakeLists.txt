# Đường dẫn file gperf trung gian và header sinh tự động trong build tree
set(GPERF_FILE ${CMAKE_CURRENT_BINARY_DIR}/dict.gperf)
set(GENERATED_HEADER ${CMAKE_CURRENT_BINARY_DIR}/syllable_hash.hpp)

# File đầu vào từ thư mục data/ và scripts/
set(DICT_INPUT ${CMAKE_SOURCE_DIR}/data/vietnamese.cm.dict)
set(GEN_SCRIPT ${CMAKE_SOURCE_DIR}/scripts/generate_dict.py)

# Custom Command 1: Tạo file gperf từ từ điển
add_custom_command(
        OUTPUT ${GPERF_FILE}
        COMMAND ${Python3_EXECUTABLE} ${GEN_SCRIPT} ${DICT_INPUT} ${GPERF_FILE}
        DEPENDS ${DICT_INPUT} ${GEN_SCRIPT}
        COMMENT "Generating gperf input file..."
)

# Custom Command 2: Biên dịch file gperf thành header C++
add_custom_command(
        OUTPUT ${GENERATED_HEADER}
        COMMAND ${GPERF_EXECUTABLE} -t -C -N in_word_set -L C++ ${GPERF_FILE} > ${GENERATED_HEADER}
        DEPENDS ${GPERF_FILE}
        COMMENT "Generating static perfect hash dictionary with gperf..."
)
# Đánh dấu cho IDE biết đây là file tự sinh
set_source_files_properties(${GENERATED_HEADER} PROPERTIES GENERATED TRUE)

add_library(vipy MODULE
        # Headers (Khai báo để CLion nhận diện project target)
        ${CMAKE_SOURCE_DIR}/include/vipy/syllable_dict.hpp
        ${CMAKE_SOURCE_DIR}/include/vipy/utf8_helper.hpp
        ${CMAKE_SOURCE_DIR}/include/vipy/vipy_config.hpp
        ${CMAKE_SOURCE_DIR}/include/vipy/vipy-icon-resolver.hpp

        # Sources
        core/syllable_dict.cpp
        fcitx5/vipy-icon-resolver.cpp
        fcitx5/vipy_addon.cpp

        # Generated header
        ${GENERATED_HEADER}
)

# Định nghĩa Include Directories
target_include_directories(vipy PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_CURRENT_BINARY_DIR}          # Để bao hàm file sinh tự động <syllable_hash.hpp>
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