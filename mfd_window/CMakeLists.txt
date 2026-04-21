# This file is part of MFDStudio.
# Project author: Benoit Fra
# Repository: https://github.com/benoitfragit/MFDStudio
set(MFD_WINDOW_LAUNCHER_HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/include/mfd/window/WindowLauncher.h)

set(MFD_WINDOW_LAUNCHER_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/WindowLauncher.cpp)

add_library(mfd_window_launcher STATIC
    ${MFD_WINDOW_LAUNCHER_HEADERS}
    ${MFD_WINDOW_LAUNCHER_SOURCES})

add_library(mfd::window_launcher ALIAS mfd_window_launcher)

target_include_directories(mfd_window_launcher
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_libraries(mfd_window_launcher
    PUBLIC
        mfd::api)

set(MFD_WINDOW_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp)

add_executable(mfd_window ${MFD_WINDOW_SOURCES})

target_link_libraries(mfd_window
    PRIVATE
        mfd::window_launcher)

set_target_properties(mfd_window PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:mfd_window>")

if(WIN32)
    add_custom_command(
        TARGET mfd_window
        POST_BUILD
        COMMAND
            ${CMAKE_COMMAND}
            "-DSOURCE_DIR=${MFD_ROOT_DIR}/assets"
            "-DDEST_DIR=$<TARGET_FILE_DIR:mfd_window>/assets"
            -P
            "${MFD_ROOT_DIR}/cmake/SyncAssetTree.cmake"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_RUNTIME_DLLS:mfd_window>
            $<TARGET_RUNTIME_DLLS:mfd_api>
            "$<TARGET_FILE_DIR:mfd_window>"
        COMMAND_EXPAND_LISTS)
else()
    add_custom_command(
        TARGET mfd_window
        POST_BUILD
        COMMAND
            ${CMAKE_COMMAND}
            "-DSOURCE_DIR=${MFD_ROOT_DIR}/assets"
            "-DDEST_DIR=$<TARGET_FILE_DIR:mfd_window>/assets"
            -P
            "${MFD_ROOT_DIR}/cmake/SyncAssetTree.cmake")
endif()

mfd_stage_runtime(mfd_window WITH_ASSETS WITH_RUNTIME_DLLS)
