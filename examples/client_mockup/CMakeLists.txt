# This file is part of MFDStudio.
# Project author: Benoit Fra
# Repository: https://github.com/benoitfragit/MFDStudio
set(MFD_MOCKUP_GENERATED_DIR
    ${CMAKE_CURRENT_SOURCE_DIR}/generated)

set(MFD_MOCKUP_FULL_DEMO_HEADER
    ${MFD_MOCKUP_GENERATED_DIR}/FullDemoMockupUi.h)
set(MFD_MOCKUP_FULL_DEMO_SOURCE
    ${MFD_MOCKUP_GENERATED_DIR}/FullDemoMockupUi.cpp)

set(MFD_MOCKUP_COCKPIT_DEMO_HEADER
    ${MFD_MOCKUP_GENERATED_DIR}/CockpitDemoMockupUi.h)
set(MFD_MOCKUP_COCKPIT_DEMO_SOURCE
    ${MFD_MOCKUP_GENERATED_DIR}/CockpitDemoMockupUi.cpp)

set(MFD_MOCKUP_MINIMAL_RADAR_HEADER
    ${MFD_MOCKUP_GENERATED_DIR}/MinimalRadarMockupUi.h)
set(MFD_MOCKUP_MINIMAL_RADAR_SOURCE
    ${MFD_MOCKUP_GENERATED_DIR}/MinimalRadarMockupUi.cpp)

client_api_generate_ui(
    WINDOW_JSON "assets/windows/demo_pages.json"
    OUTPUT_HEADER "${MFD_MOCKUP_FULL_DEMO_HEADER}"
    OUTPUT_SOURCE "${MFD_MOCKUP_FULL_DEMO_SOURCE}"
    OUTPUT_MAP "${MFD_ROOT_DIR}/assets/windows/demo_pages.generated.map"
    NAMESPACE "full_demo_ui"
    UI_CLASS_NAME "FullDemoMockupUi"
    HEADER_INCLUDE "FullDemoMockupUi.h")

client_api_generate_ui(
    WINDOW_JSON "assets/windows/demo_pages_cockpit.json"
    OUTPUT_HEADER "${MFD_MOCKUP_COCKPIT_DEMO_HEADER}"
    OUTPUT_SOURCE "${MFD_MOCKUP_COCKPIT_DEMO_SOURCE}"
    OUTPUT_MAP "${MFD_ROOT_DIR}/assets/windows/demo_pages_cockpit.generated.map"
    NAMESPACE "cockpit_demo_ui"
    UI_CLASS_NAME "CockpitDemoMockupUi"
    HEADER_INCLUDE "CockpitDemoMockupUi.h")

client_api_generate_ui(
    WINDOW_JSON "assets/windows/demo_pages_minimal.json"
    OUTPUT_HEADER "${MFD_MOCKUP_MINIMAL_RADAR_HEADER}"
    OUTPUT_SOURCE "${MFD_MOCKUP_MINIMAL_RADAR_SOURCE}"
    OUTPUT_MAP "${MFD_ROOT_DIR}/assets/windows/demo_pages_minimal.generated.map"
    NAMESPACE "minimal_radar_ui"
    UI_CLASS_NAME "MinimalRadarMockupUi"
    HEADER_INCLUDE "MinimalRadarMockupUi.h")

set(MFD_MOCKUP_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp
    ${MFD_MOCKUP_FULL_DEMO_SOURCE}
    ${MFD_MOCKUP_COCKPIT_DEMO_SOURCE}
    ${MFD_MOCKUP_MINIMAL_RADAR_SOURCE})

set(MFD_MOCKUP_HEADERS
    ${MFD_MOCKUP_FULL_DEMO_HEADER}
    ${MFD_MOCKUP_COCKPIT_DEMO_HEADER}
    ${MFD_MOCKUP_MINIMAL_RADAR_HEADER})

add_executable(client_mockup
    ${MFD_MOCKUP_HEADERS}
    ${MFD_MOCKUP_SOURCES})

target_include_directories(client_mockup
    PRIVATE
        ${MFD_MOCKUP_GENERATED_DIR})

target_link_libraries(client_mockup
    PRIVATE
        mfd::api
        mfd::client)

mfd_begin_external_libraries(client_mockup)
mfd_private_library(client_mockup imgui)
mfd_private_library(client_mockup raylib)
mfd_private_library(client_mockup rlimgui)
mfd_end_external_libraries(client_mockup)

set_target_properties(client_mockup PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:client_mockup>")

set(MFD_CLIENT_MOCKUP_STAGE_DIR
    "${MFD_ROOT_DIR}/_Exec/${MFD_EXEC_COMPILER_DIR}/${MFD_EXEC_PLATFORM_DIR}/$<CONFIG>")
set(MFD_CLIENT_MOCKUP_API_RUNTIME_DLLS_ARG
    "-DRUNTIME_DLLS=$<JOIN:$<TARGET_RUNTIME_DLLS:mfd_api>,$<SEMICOLON>>")

add_custom_command(
    TARGET client_mockup
    POST_BUILD
    COMMAND
        ${CMAKE_COMMAND}
        "-DSOURCE_DIR=${MFD_ROOT_DIR}/assets"
        "-DDEST_DIR=$<TARGET_FILE_DIR:client_mockup>/assets"
        -P
        "${MFD_ROOT_DIR}/cmake/SyncAssetTree.cmake"
    COMMAND
        ${CMAKE_COMMAND}
        "-DDEST_DIR=$<TARGET_FILE_DIR:client_mockup>"
        "-DRUNTIME_DLLS=$<JOIN:$<TARGET_RUNTIME_DLLS:client_mockup>,$<SEMICOLON>>"
        -P
        "${MFD_ROOT_DIR}/cmake/CopyRuntimeDependencies.cmake"
    COMMAND
        ${CMAKE_COMMAND}
        "-DDEST_DIR=$<TARGET_FILE_DIR:client_mockup>"
        "${MFD_CLIENT_MOCKUP_API_RUNTIME_DLLS_ARG}"
        -P
        "${MFD_ROOT_DIR}/cmake/CopyRuntimeDependencies.cmake"
    COMMAND
        ${CMAKE_COMMAND}
        "-DDEST_DIR=${MFD_CLIENT_MOCKUP_STAGE_DIR}"
        "${MFD_CLIENT_MOCKUP_API_RUNTIME_DLLS_ARG}"
        -P
        "${MFD_ROOT_DIR}/cmake/CopyRuntimeDependencies.cmake"
    VERBATIM)

mfd_stage_runtime(client_mockup WITH_ASSETS WITH_RUNTIME_DLLS)

if(MFD_BUILD_TESTS)
    mfd_add_runtime_layout_smoke_test(
        client_mockup
        EXTRA_RUNTIME_TARGETS mfd_api
        VERIFY_ASSETS
        LABELS examples)
endif()
