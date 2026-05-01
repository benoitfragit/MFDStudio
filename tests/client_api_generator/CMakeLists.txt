# This file is part of MFDStudio.
# Project author: Benoit Fra
# Repository: https://github.com/benoitfragit/MFDStudio
find_package(Python3 COMPONENTS Interpreter REQUIRED)

add_test(
    NAME client_api_generator_tests
    COMMAND
        ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/test_generate_ui.py
        --generator
        ${MFD_ROOT_DIR}/mfd_client_api/generator/scripts/generate_ui.py)
