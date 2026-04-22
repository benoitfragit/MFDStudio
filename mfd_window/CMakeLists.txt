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

set(MFD_WINDOW_STAGE_DIR "${MFD_ROOT_DIR}/_Exec/${MFD_EXEC_COMPILER_DIR}/${MFD_EXEC_PLATFORM_DIR}/$<CONFIG>")
set(MFD_WINDOW_LAUNCH_SCRIPTS
    ${MFD_ROOT_DIR}/Start-MfdWindow.bat
    ${MFD_ROOT_DIR}/Start-MfdDemo.bat
    ${MFD_ROOT_DIR}/Start-MfdCockpit.bat
    ${MFD_ROOT_DIR}/Start-MfdMinimal.bat
    ${MFD_ROOT_DIR}/Start-MfdTutorial.bat)

set_source_files_properties(${MFD_WINDOW_LAUNCH_SCRIPTS} PROPERTIES HEADER_FILE_ONLY TRUE)
target_sources(mfd_window PRIVATE ${MFD_WINDOW_LAUNCH_SCRIPTS})

set(MFD_WINDOW_SCRIPT_COPY_COMMANDS)
foreach(mfd_window_script IN LISTS MFD_WINDOW_LAUNCH_SCRIPTS)
    list(APPEND MFD_WINDOW_SCRIPT_COPY_COMMANDS
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${mfd_window_script}"
            "$<TARGET_FILE_DIR:mfd_window>")
    list(APPEND MFD_WINDOW_SCRIPT_COPY_COMMANDS
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${mfd_window_script}"
            "${MFD_WINDOW_STAGE_DIR}")
endforeach()

set(MFD_WINDOW_SCRIPT_CLEANUP_COMMANDS
    COMMAND ${CMAKE_COMMAND} -E rm -f
        "$<TARGET_FILE_DIR:mfd_window>/Start-MfdWindow.ps1"
        "$<TARGET_FILE_DIR:mfd_window>/Start-MfdDemo.ps1"
        "$<TARGET_FILE_DIR:mfd_window>/Start-MfdCockpit.ps1"
        "$<TARGET_FILE_DIR:mfd_window>/Start-MfdMinimal.ps1"
        "$<TARGET_FILE_DIR:mfd_window>/Start-MfdTutorial.ps1"
    COMMAND ${CMAKE_COMMAND} -E rm -f
        "${MFD_WINDOW_STAGE_DIR}/Start-MfdWindow.ps1"
        "${MFD_WINDOW_STAGE_DIR}/Start-MfdDemo.ps1"
        "${MFD_WINDOW_STAGE_DIR}/Start-MfdCockpit.ps1"
        "${MFD_WINDOW_STAGE_DIR}/Start-MfdMinimal.ps1"
        "${MFD_WINDOW_STAGE_DIR}/Start-MfdTutorial.ps1")

if(WIN32)
    add_custom_command(
        TARGET mfd_window
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "${MFD_WINDOW_STAGE_DIR}"
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
        ${MFD_WINDOW_SCRIPT_CLEANUP_COMMANDS}
        ${MFD_WINDOW_SCRIPT_COPY_COMMANDS}
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
