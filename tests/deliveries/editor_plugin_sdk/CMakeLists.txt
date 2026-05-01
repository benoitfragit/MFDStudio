# This file is part of MFDStudio.
# Project author: Benoit Fra
# Repository: https://github.com/benoitfragit/MFDStudio
cmake_minimum_required(VERSION 3.25)

project(MFDStudioEditorPluginPackageSmoke LANGUAGES C)

if(NOT DEFINED MFD_PACKAGE_CMAKE_DIR OR MFD_PACKAGE_CMAKE_DIR STREQUAL "")
    message(FATAL_ERROR "MFD_PACKAGE_CMAKE_DIR must point to the staged MFDStudioEditorPlugin package.")
endif()

find_package(MFDStudioEditorPlugin REQUIRED CONFIG
    PATHS "${MFD_PACKAGE_CMAKE_DIR}"
    NO_DEFAULT_PATH)

add_library(mfd_editor_plugin_package_smoke SHARED
    "${CMAKE_CURRENT_SOURCE_DIR}/EditorPlugin.c")

target_link_libraries(mfd_editor_plugin_package_smoke
    PRIVATE
        MFDStudio::EditorPluginApi)

target_compile_definitions(mfd_editor_plugin_package_smoke
    PRIVATE
        MFD_EDITOR_AUTOMATION_PLUGIN_EXPORTS=1)
