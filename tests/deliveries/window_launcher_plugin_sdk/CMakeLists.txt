# This file is part of MFDStudio.
# Project author: Benoit Fra
# Repository: https://github.com/benoitfragit/MFDStudio
cmake_minimum_required(VERSION 3.25)

project(MFDStudioWindowLauncherPluginPackageSmoke LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT DEFINED MFD_PACKAGE_CMAKE_DIR OR MFD_PACKAGE_CMAKE_DIR STREQUAL "")
    message(FATAL_ERROR "MFD_PACKAGE_CMAKE_DIR must point to the staged MFDStudioWindowLauncherPlugin package.")
endif()

find_package(MFDStudioWindowLauncherPlugin REQUIRED CONFIG
    PATHS "${MFD_PACKAGE_CMAKE_DIR}"
    NO_DEFAULT_PATH)

add_library(mfd_window_launcher_plugin_package_smoke SHARED
    "${CMAKE_CURRENT_SOURCE_DIR}/FramebufferPlugin.cpp")

target_link_libraries(mfd_window_launcher_plugin_package_smoke
    PRIVATE
        MFDStudio::WindowLauncherPlugin)
