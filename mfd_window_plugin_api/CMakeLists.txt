# This file is part of MFDStudio.
# Project author: Benoit Fra
# Repository: https://github.com/benoitfragit/MFDStudio

set(MFD_WINDOW_PLUGIN_API_PUBLIC_HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/include/mfd/window/WindowLauncherPlugin.h)

set(MFD_WINDOW_PLUGIN_API_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/WindowLauncherPlugin.cpp)

add_library(mfd_window_plugin_api SHARED
    ${MFD_WINDOW_PLUGIN_API_PUBLIC_HEADERS}
    ${MFD_WINDOW_PLUGIN_API_SOURCES})

add_library(mfd::window_plugin_api ALIAS mfd_window_plugin_api)

target_include_directories(mfd_window_plugin_api
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_compile_features(mfd_window_plugin_api PUBLIC cxx_std_17)
target_compile_definitions(mfd_window_plugin_api
    PRIVATE
        MFD_WINDOW_PLUGIN_API_EXPORTS)

mfd_enable_target_warnings(mfd_window_plugin_api)
