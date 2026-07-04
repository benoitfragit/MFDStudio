# This file is part of MFDStudio.
# Project author: Benoit Fra
# Repository: https://github.com/benoitfragit/MFDStudio
set(MFD_RADAR_LOAD_ASSET_DIR
    "${CMAKE_CURRENT_SOURCE_DIR}/../assets")

set(MFD_RADAR_LOAD_GENERATED_DIR
    ${CMAKE_CURRENT_SOURCE_DIR}/generated)

set(MFD_RADAR_LOAD_GENERATED_HEADER
    ${MFD_RADAR_LOAD_GENERATED_DIR}/RadarLoadUi.h)

set(MFD_RADAR_LOAD_GENERATED_SOURCE
    ${MFD_RADAR_LOAD_GENERATED_DIR}/RadarLoadUi.cpp)

client_api_generate_ui(
    WINDOW_JSON "${MFD_RADAR_LOAD_ASSET_DIR}/windows/radar_load_window.json"
    OUTPUT_HEADER "${MFD_RADAR_LOAD_GENERATED_HEADER}"
    OUTPUT_SOURCE "${MFD_RADAR_LOAD_GENERATED_SOURCE}"
    OUTPUT_MAP "${MFD_RADAR_LOAD_ASSET_DIR}/windows/radar_load_window.generated.map"
    NAMESPACE "radar_load_ui"
    UI_CLASS_NAME "RadarLoadUi"
    HEADER_INCLUDE "RadarLoadUi.h")

add_executable(radar_load_client
    ${MFD_RADAR_LOAD_GENERATED_HEADER}
    ${MFD_RADAR_LOAD_GENERATED_SOURCE}
    ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp)

target_include_directories(radar_load_client
    PRIVATE
        ${MFD_RADAR_LOAD_GENERATED_DIR})

target_link_libraries(radar_load_client
    PRIVATE
        mfd::client)

mfd_enable_target_warnings(radar_load_client)

set_target_properties(radar_load_client PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:radar_load_client>")

add_custom_command(
    TARGET radar_load_client
    POST_BUILD
    COMMAND
        ${CMAKE_COMMAND}
        "-DSOURCE_DIR=${MFD_RADAR_LOAD_ASSET_DIR}"
        "-DDEST_DIR=$<TARGET_FILE_DIR:radar_load_client>/assets"
        -P
        "${MFD_ROOT_DIR}/cmake/SyncAssetTree.cmake"
    COMMAND
        ${CMAKE_COMMAND}
        "-DDEST_DIR=$<TARGET_FILE_DIR:radar_load_client>"
        "-DRUNTIME_DLLS=$<JOIN:$<TARGET_RUNTIME_DLLS:radar_load_client>,$<SEMICOLON>>"
        -P
        "${MFD_ROOT_DIR}/cmake/CopyRuntimeDependencies.cmake"
    VERBATIM)

mfd_stage_runtime(
    radar_load_client
    ROOT_ASSET_DIRS "${MFD_RADAR_LOAD_ASSET_DIR}"
    WITH_RUNTIME_DLLS
    SCRIPT_SOURCE_DIR "${MFD_ROOT_DIR}/Scripts"
    SCRIPT_NAMES Start-RadarLoad.bat Start-MfdWindow.bat)

if(MFD_BUILD_TESTS)
    mfd_add_runtime_layout_smoke_test(
        radar_load_client
        VERIFY_ASSETS
        ASSET_SOURCE_DIR "${MFD_RADAR_LOAD_ASSET_DIR}"
        EXTRA_FILES
            assets/windows/radar_load_window.json
        LABELS examples)
endif()
