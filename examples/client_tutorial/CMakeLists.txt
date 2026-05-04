# This file is part of MFDStudio.
# Project author: Benoit Fra
# Repository: https://github.com/benoitfragit/MFDStudio
set(MFD_CLIENT_TUTORIAL_WINDOW_JSON
    ${MFD_ROOT_DIR}/assets/windows/mfd_tutorial.json)

set(MFD_CLIENT_TUTORIAL_REQUIRED_FILES
    "${MFD_CLIENT_TUTORIAL_WINDOW_JSON}"
    "${MFD_ROOT_DIR}/assets/pages/mfd_tutorial_page1.json"
    "${MFD_ROOT_DIR}/assets/pages/mfd_tutorial_page2.json"
    "${MFD_ROOT_DIR}/assets/reticles/inspired_steering_cue.json"
    "${MFD_ROOT_DIR}/assets/reticles/mfd_tutorial_radar_track.json"
    "${MFD_ROOT_DIR}/assets/reticles/mfd_tutorial_circle.json"
    "${MFD_ROOT_DIR}/assets/reticles/mfd_tutorial_progress_bar.json"
    "${MFD_ROOT_DIR}/assets/reticles/mfd_tutorial_strobe_cursor.json")

set(MFD_CLIENT_TUTORIAL_MISSING_FILES)
foreach(required_file IN LISTS MFD_CLIENT_TUTORIAL_REQUIRED_FILES)
    if(NOT EXISTS "${required_file}")
        list(APPEND MFD_CLIENT_TUTORIAL_MISSING_FILES "${required_file}")
    endif()
endforeach()

if(MFD_CLIENT_TUTORIAL_MISSING_FILES)
    string(JOIN "\n  - " MFD_CLIENT_TUTORIAL_MISSING_FILES_TEXT ${MFD_CLIENT_TUTORIAL_MISSING_FILES})
    message(STATUS
        "Skipping client_tutorial because the tutorial asset set is incomplete.\n"
        "  - ${MFD_CLIENT_TUTORIAL_MISSING_FILES_TEXT}\n"
        "Finish the editor tutorial and save the authored assets before reconfiguring.")
    return()
endif()

set(MFD_CLIENT_TUTORIAL_GENERATED_DIR
    ${CMAKE_CURRENT_SOURCE_DIR}/generated)

set(MFD_CLIENT_TUTORIAL_GENERATED_HEADER
    ${MFD_CLIENT_TUTORIAL_GENERATED_DIR}/TutorialUi.h)

set(MFD_CLIENT_TUTORIAL_GENERATED_SOURCE
    ${MFD_CLIENT_TUTORIAL_GENERATED_DIR}/TutorialUi.cpp)

client_api_generate_ui(
    WINDOW_JSON "${MFD_CLIENT_TUTORIAL_WINDOW_JSON}"
    OUTPUT_HEADER "${MFD_CLIENT_TUTORIAL_GENERATED_HEADER}"
    OUTPUT_SOURCE "${MFD_CLIENT_TUTORIAL_GENERATED_SOURCE}"
    OUTPUT_MAP "${MFD_ROOT_DIR}/assets/windows/mfd_tutorial.generated.map"
    NAMESPACE "tutorial_ui"
    UI_CLASS_NAME "TutorialUi"
    HEADER_INCLUDE "TutorialUi.h")

set(MFD_CLIENT_TUTORIAL_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp
    ${MFD_CLIENT_TUTORIAL_GENERATED_SOURCE})

set(MFD_CLIENT_TUTORIAL_HEADERS
    ${MFD_CLIENT_TUTORIAL_GENERATED_HEADER})

add_executable(client_tutorial
    ${MFD_CLIENT_TUTORIAL_HEADERS}
    ${MFD_CLIENT_TUTORIAL_SOURCES})

target_include_directories(client_tutorial
    PRIVATE
        ${MFD_CLIENT_TUTORIAL_GENERATED_DIR})

target_link_libraries(client_tutorial
    PRIVATE
        mfd::client
        mfd::io_json)

set_target_properties(client_tutorial PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:client_tutorial>")

add_custom_command(
    TARGET client_tutorial
    POST_BUILD
    COMMAND
        ${CMAKE_COMMAND}
        "-DSOURCE_DIR=${MFD_ROOT_DIR}/assets"
        "-DDEST_DIR=$<TARGET_FILE_DIR:client_tutorial>/assets"
        -P
        "${MFD_ROOT_DIR}/cmake/SyncAssetTree.cmake"
    COMMAND
        ${CMAKE_COMMAND}
        "-DDEST_DIR=$<TARGET_FILE_DIR:client_tutorial>"
        "-DRUNTIME_DLLS=$<JOIN:$<TARGET_RUNTIME_DLLS:client_tutorial>,$<SEMICOLON>>"
        -P
        "${MFD_ROOT_DIR}/cmake/CopyRuntimeDependencies.cmake"
    VERBATIM)

mfd_stage_runtime(client_tutorial WITH_ASSETS WITH_RUNTIME_DLLS)

if(MFD_BUILD_TESTS)
    mfd_add_runtime_layout_smoke_test(
        client_tutorial
        VERIFY_ASSETS
        LABELS examples)
endif()
