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
        mfd::client
        imgui
        rlimgui)

set_target_properties(client_mockup PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:client_mockup>")

add_custom_command(
    TARGET client_mockup
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${MFD_ROOT_DIR}/assets"
        "$<TARGET_FILE_DIR:client_mockup>/assets"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_RUNTIME_DLLS:client_mockup>
        "$<TARGET_FILE_DIR:client_mockup>"
    COMMAND_EXPAND_LISTS)

mfd_stage_runtime(client_mockup WITH_ASSETS WITH_RUNTIME_DLLS)
