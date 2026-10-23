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

add_test(NAME widget_generator_tests
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/test_generate_widgets.py)

add_test(NAME widget_showcase_assets_check
    COMMAND ${Python3_EXECUTABLE}
        ${MFD_ROOT_DIR}/examples/apilevel1/widgets/generate_showcase_assets.py
        --check)
