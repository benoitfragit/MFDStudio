# This file is part of MFDStudio.
# Project author: Benoit Fra
# Repository: https://github.com/benoitfragit/MFDStudio

add_library(mfd_editor_plugin_api INTERFACE)
add_library(mfd::editor_plugin_api ALIAS mfd_editor_plugin_api)

target_include_directories(mfd_editor_plugin_api
    INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_compile_features(mfd_editor_plugin_api INTERFACE cxx_std_17)

mfd_register_dev_package(
    NAME MFDStudioEditorPlugin
    HEADER_ONLY
    IMPORTED_TARGET_NAME MFDStudio::EditorPluginApi
    PUBLIC_INCLUDE_DIRS
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
    DOCUMENTATION_FILES
        "${CMAKE_CURRENT_SOURCE_DIR}/README.md"
    EXAMPLE_DIRECTORIES
        "${MFD_ROOT_DIR}/examples/mfd_editor_automation_sample_plugin"
    CONFIG_TEMPLATE
        "${MFD_ROOT_DIR}/cmake/packages/MFDStudioEditorPluginConfig.cmake.in")
