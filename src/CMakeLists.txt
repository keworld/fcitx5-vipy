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

# Thêm module thư viện vicplex bao gồm các file source trong src/core và src/fcitx5
add_library(vicplex MODULE
        # Headers (Khai báo để CLion nhận diện project target)
        ${CMAKE_SOURCE_DIR}/include/vicplex/char_buffer.hpp
        ${CMAKE_SOURCE_DIR}/include/vicplex/char_util.hpp
        ${CMAKE_SOURCE_DIR}/include/vicplex/syllable_dict.hpp
        ${CMAKE_SOURCE_DIR}/include/vicplex/telex_data.hpp
        ${CMAKE_SOURCE_DIR}/include/vicplex/telex_transformer.hpp
        ${CMAKE_SOURCE_DIR}/include/vicplex/utf8_helper.hpp
        ${CMAKE_SOURCE_DIR}/include/vicplex/vni_data.hpp
        ${CMAKE_SOURCE_DIR}/include/vicplex/vni_transformer.hpp
        ${CMAKE_SOURCE_DIR}/include/vicplex/vicplex_config.hpp

        # Sources
        core/syllable_dict.cpp
        core/telex_data.cpp
        core/telex_transformer.cpp
        core/vni_data.cpp
        core/vni_transformer.cpp
        fcitx5/vicplex-icon-resolver.cpp
        fcitx5/vicplex_addon.cpp

        # Generated header
        ${GENERATED_HEADER}
)

# Định nghĩa Include Directories
target_include_directories(vicplex PRIVATE
        ${CMAKE_SOURCE_DIR}/include          # Để bao hàm <vicplex/header.hpp>
        ${CMAKE_CURRENT_BINARY_DIR}          # Để bao hàm file sinh tự động <syllable_hash.hpp>
)
target_compile_definitions(vicplex PRIVATE
        FCITX_VICPLEX_ICON_DIR="${CMAKE_INSTALL_FULL_DATADIR}/icons/hicolor/scalable/apps"
)

# Link thư viện Fcitx5
target_link_libraries(vicplex PRIVATE Fcitx5::Core)

# Loại bỏ tiền tố 'lib' mặc định để sinh ra vicplex.so thay vì libvicplex.so
set_target_properties(vicplex PROPERTIES PREFIX "")

# Cài đặt target module vào thư mục addon do Fcitx5 xác định.
# Biến này cũng xử lý đúng prefix tùy chỉnh và cấu hình multiarch của distro.
install(TARGETS vicplex DESTINATION ${FCITX_INSTALL_ADDONDIR})