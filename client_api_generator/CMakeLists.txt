# This file is part of MFDStudio.
# Project author: Benoit Fra
# Repository: https://github.com/benoitfragit/MFDStudio
add_custom_target(client_api_generator
    SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/ClientApiGenerator.cmake
        ${CMAKE_CURRENT_SOURCE_DIR}/scripts/generate_ui.py)

if(MFD_BUILD_TESTS)
    add_subdirectory(tests)
endif()
