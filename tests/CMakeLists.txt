add_test(NAME python_engine_tests
        COMMAND ${Python3_EXECUTABLE} -m unittest vietnamese_input_method_test)
set_tests_properties(python_engine_tests PROPERTIES
        ENVIRONMENT "PYTHONPATH=${CMAKE_SOURCE_DIR}/scripts")
