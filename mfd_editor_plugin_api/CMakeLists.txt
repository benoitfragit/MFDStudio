# This file is part of MFDStudio.
# Project author: Benoit Fra
# Repository: https://github.com/benoitfragit/MFDStudio

add_library(mfd_editor_plugin_api INTERFACE)
add_library(mfd::editor_plugin_api ALIAS mfd_editor_plugin_api)

target_include_directories(mfd_editor_plugin_api
    INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_compile_features(mfd_editor_plugin_api INTERFACE cxx_std_17)
