# This file is part of MFDStudio.
# Project author: Benoit Fra
# Repository: https://github.com/benoitfragit/MFDStudio
set(MFD_TUTORIAL_ASSET_DIR
    "${CMAKE_CURRENT_SOURCE_DIR}/../assets")

set(MFD_TUTORIAL_WINDOW_JSON
    ${MFD_TUTORIAL_ASSET_DIR}/windows/mfd_tutorial.json)

set(MFD_TUTORIAL_REQUIRED_FILES
    "${MFD_TUTORIAL_WINDOW_JSON}"
    "${MFD_TUTORIAL_ASSET_DIR}/pages/mfd_tutorial_page1.json"
    "${MFD_TUTORIAL_ASSET_DIR}/pages/mfd_tutorial_page2.json"
    "${MFD_TUTORIAL_ASSET_DIR}/reticles/inspired_steering_cue.json"
    "${MFD_TUTORIAL_ASSET_DIR}/reticles/mfd_tutorial_radar_track.json"
    "${MFD_TUTORIAL_ASSET_DIR}/reticles/mfd_tutorial_aircraft.json"
    "${MFD_TUTORIAL_ASSET_DIR}/reticles/mfd_tutorial_circle.json"
    "${MFD_TUTORIAL_ASSET_DIR}/reticles/mfd_tutorial_progress_bar.json"
    "${MFD_TUTORIAL_ASSET_DIR}/reticles/mfd_tutorial_strobe_cursor.json")

set(MFD_TUTORIAL_MISSING_FILES)
foreach(required_file IN LISTS MFD_TUTORIAL_REQUIRED_FILES)
    if(NOT EXISTS "${required_file}")
        list(APPEND MFD_TUTORIAL_MISSING_FILES "${required_file}")
    endif()
endforeach()

if(MFD_TUTORIAL_MISSING_FILES)
    string(JOIN "\n  - " MFD_TUTORIAL_MISSING_FILES_TEXT ${MFD_TUTORIAL_MISSING_FILES})
    message(STATUS
        "Skipping tutorial_client because the tutorial asset set is incomplete.\n"
        "  - ${MFD_TUTORIAL_MISSING_FILES_TEXT}\n"
        "Author or restore the tutorial assets under examples/tutorial/assets/, then re-run CMake configuration.")
    return()
endif()

set(MFD_TUTORIAL_GENERATED_DIR
    ${CMAKE_CURRENT_SOURCE_DIR}/generated)

set(MFD_TUTORIAL_GENERATED_HEADER
    ${MFD_TUTORIAL_GENERATED_DIR}/TutorialUi.h)

set(MFD_TUTORIAL_GENERATED_SOURCE
    ${MFD_TUTORIAL_GENERATED_DIR}/TutorialUi.cpp)

client_api_generate_ui(
    WINDOW_JSON "${MFD_TUTORIAL_WINDOW_JSON}"
    OUTPUT_HEADER "${MFD_TUTORIAL_GENERATED_HEADER}"
    OUTPUT_SOURCE "${MFD_TUTORIAL_GENERATED_SOURCE}"
    OUTPUT_MAP "${MFD_TUTORIAL_ASSET_DIR}/windows/mfd_tutorial.generated.map"
    NAMESPACE "tutorial_ui"
    UI_CLASS_NAME "TutorialUi"
    HEADER_INCLUDE "TutorialUi.h")

set(MFD_TUTORIAL_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp
    ${MFD_TUTORIAL_GENERATED_SOURCE})

set(MFD_TUTORIAL_HEADERS
    ${MFD_TUTORIAL_GENERATED_HEADER})

add_executable(tutorial_client
    ${MFD_TUTORIAL_HEADERS}
    ${MFD_TUTORIAL_SOURCES})

target_include_directories(tutorial_client
    PRIVATE
        ${MFD_TUTORIAL_GENERATED_DIR})

target_link_libraries(tutorial_client
    PRIVATE
        mfd::client)

set_target_properties(tutorial_client PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:tutorial_client>")

add_custom_command(
    TARGET tutorial_client
    POST_BUILD
    COMMAND
        ${CMAKE_COMMAND}
        "-DSOURCE_DIR=${MFD_TUTORIAL_ASSET_DIR}"
        "-DDEST_DIR=$<TARGET_FILE_DIR:tutorial_client>/assets"
        -P
        "${MFD_ROOT_DIR}/cmake/SyncAssetTree.cmake"
    COMMAND
        ${CMAKE_COMMAND}
        "-DDEST_DIR=$<TARGET_FILE_DIR:tutorial_client>"
        "-DRUNTIME_DLLS=$<JOIN:$<TARGET_RUNTIME_DLLS:tutorial_client>,$<SEMICOLON>>"
        -P
        "${MFD_ROOT_DIR}/cmake/CopyRuntimeDependencies.cmake"
    VERBATIM)

mfd_stage_runtime(
    tutorial_client
    ROOT_ASSET_DIRS "${MFD_TUTORIAL_ASSET_DIR}"
    WITH_RUNTIME_DLLS
    SCRIPT_SOURCE_DIR "${MFD_ROOT_DIR}/Scripts"
    SCRIPT_NAMES Start-Tutorial.bat Start-MfdWindow.bat)

if(MFD_BUILD_TESTS)
    mfd_add_runtime_layout_smoke_test(
        tutorial_client
        VERIFY_ASSETS
        ASSET_SOURCE_DIR "${MFD_TUTORIAL_ASSET_DIR}"
        LABELS examples)
endif()
