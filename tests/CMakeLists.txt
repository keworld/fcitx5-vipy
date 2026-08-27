add_executable(transformer_tests
        transformer_test.cpp
        ${CMAKE_SOURCE_DIR}/src/core/syllable_dict.cpp
        ${CMAKE_SOURCE_DIR}/src/core/telex_data.cpp
        ${CMAKE_SOURCE_DIR}/src/core/telex_transformer.cpp
        ${CMAKE_SOURCE_DIR}/src/core/vni_data.cpp
        ${CMAKE_SOURCE_DIR}/src/core/vni_transformer.cpp
)

target_include_directories(transformer_tests PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_BINARY_DIR}/src
)

add_dependencies(transformer_tests vicplex)
add_test(NAME transformer_tests COMMAND transformer_tests)
