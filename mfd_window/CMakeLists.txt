# This file is part of MFDStudio.
# Project author: Benoit Fra
# Repository: https://github.com/benoitfragit/MFDStudio
set(MFD_WINDOW_LAUNCHER_HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/include/mfd/window/WindowLauncher.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/RuntimeDebugInspectorFrameState.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/RuntimeDebugOverlay.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/RuntimeDebugPreview.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/RuntimeDebugState.hpp)

set(MFD_WINDOW_LAUNCHER_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/WindowLauncher.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/RuntimeDebugOverlay.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/RuntimeDebugPreview.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/RuntimeDebugState.cpp)

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

mfd_private_library(mfd_window_launcher raylib)
mfd_public_library(mfd_window_launcher rlimgui)

mfd_enable_target_warnings(mfd_window_launcher)

set(MFD_WINDOW_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp)

set(MFD_WINDOW_WINDOWS_RESOURCES)
if(WIN32)
    list(APPEND MFD_WINDOW_WINDOWS_RESOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/resources/mfd_window.rc)
endif()

add_executable(mfd_window
    ${MFD_WINDOW_SOURCES}
    ${MFD_WINDOW_WINDOWS_RESOURCES})

target_link_libraries(mfd_window
    PRIVATE
        mfd::window_launcher
        raylib)

set_target_properties(mfd_window PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:mfd_window>")

mfd_enable_target_warnings(mfd_window)

set(MFD_WINDOW_STAGE_DIR "${MFD_ROOT_DIR}/_Exec/${MFD_EXEC_COMPILER_DIR}/${MFD_EXEC_PLATFORM_DIR}/$<CONFIG>")
set(MFD_WINDOW_API_RUNTIME_DLLS_ARG
    "-DRUNTIME_DLLS=$<JOIN:$<TARGET_RUNTIME_DLLS:mfd_api>,$<SEMICOLON>>")
set(MFD_WINDOW_SCRIPT_SOURCE_DIR "${MFD_ROOT_DIR}/Scripts")
set(MFD_WINDOW_LAUNCH_SCRIPT_NAMES
    Start-MfdWindow.bat
    Start-MfdDemo.bat
    Start-MfdCockpit.bat
    Start-MfdMinimal.bat
    Start-MfdTutorial.bat)

set(MFD_WINDOW_LAUNCH_SCRIPTS)
foreach(mfd_window_script_name IN LISTS MFD_WINDOW_LAUNCH_SCRIPT_NAMES)
    list(APPEND MFD_WINDOW_LAUNCH_SCRIPTS "${MFD_WINDOW_SCRIPT_SOURCE_DIR}/${mfd_window_script_name}")
endforeach()

string(JOIN ";" MFD_WINDOW_LAUNCH_SCRIPT_LIST ${MFD_WINDOW_LAUNCH_SCRIPT_NAMES})

set_source_files_properties(${MFD_WINDOW_LAUNCH_SCRIPTS} PROPERTIES HEADER_FILE_ONLY TRUE)
target_sources(mfd_window PRIVATE ${MFD_WINDOW_LAUNCH_SCRIPTS})

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
        COMMAND
            ${CMAKE_COMMAND}
            "-DDEST_DIR=$<TARGET_FILE_DIR:mfd_window>"
            "-DRUNTIME_DLLS=$<JOIN:$<TARGET_RUNTIME_DLLS:mfd_window>,$<SEMICOLON>>"
            -P
            "${MFD_ROOT_DIR}/cmake/CopyRuntimeDependencies.cmake"
        COMMAND
            ${CMAKE_COMMAND}
            "-DDEST_DIR=$<TARGET_FILE_DIR:mfd_window>"
            "${MFD_WINDOW_API_RUNTIME_DLLS_ARG}"
            -P
            "${MFD_ROOT_DIR}/cmake/CopyRuntimeDependencies.cmake"
        COMMAND
            ${CMAKE_COMMAND}
            "-DSOURCE_DIR=${MFD_WINDOW_SCRIPT_SOURCE_DIR}"
            "-DDEST_DIR=$<TARGET_FILE_DIR:mfd_window>"
            "-DFILE_NAMES=${MFD_WINDOW_LAUNCH_SCRIPT_LIST}"
            -P
            "${MFD_ROOT_DIR}/cmake/SyncLaunchScripts.cmake"
        COMMAND
            ${CMAKE_COMMAND}
            "-DSOURCE_DIR=${MFD_WINDOW_SCRIPT_SOURCE_DIR}"
            "-DDEST_DIR=${MFD_WINDOW_STAGE_DIR}"
            "-DFILE_NAMES=${MFD_WINDOW_LAUNCH_SCRIPT_LIST}"
            -P
            "${MFD_ROOT_DIR}/cmake/SyncLaunchScripts.cmake"
        COMMAND
            ${CMAKE_COMMAND}
            "-DDEST_DIR=${MFD_WINDOW_STAGE_DIR}"
            "${MFD_WINDOW_API_RUNTIME_DLLS_ARG}"
            -P
            "${MFD_ROOT_DIR}/cmake/CopyRuntimeDependencies.cmake"
        VERBATIM)
else()
    add_custom_command(
        TARGET mfd_window
        POST_BUILD
        COMMAND
            ${CMAKE_COMMAND}
            "-DSOURCE_DIR=${MFD_ROOT_DIR}/assets"
            "-DDEST_DIR=$<TARGET_FILE_DIR:mfd_window>/assets"
            -P
            "${MFD_ROOT_DIR}/cmake/SyncAssetTree.cmake"
        VERBATIM)
endif()

mfd_stage_runtime(mfd_window WITH_ASSETS WITH_RUNTIME_DLLS)
