# This file is part of MFDStudio.
# Project author: Benoit Fra
# Repository: https://github.com/benoitfragit/MFDStudio
set(MFD_MOCKUP_MINIMAL_GENERATED_DIR
    ${CMAKE_CURRENT_SOURCE_DIR}/generated)

set(MFD_MOCKUP_MINIMAL_GENERATED_HEADER
    ${MFD_MOCKUP_MINIMAL_GENERATED_DIR}/MockupUi.h)

set(MFD_MOCKUP_MINIMAL_GENERATED_SOURCE
    ${MFD_MOCKUP_MINIMAL_GENERATED_DIR}/MockupUi.cpp)

client_api_generate_ui(
    WINDOW_JSON "assets/windows/demo_pages_cockpit.json"
    OUTPUT_HEADER "${MFD_MOCKUP_MINIMAL_GENERATED_HEADER}"
    OUTPUT_SOURCE "${MFD_MOCKUP_MINIMAL_GENERATED_SOURCE}"
    NAMESPACE "mockup_ui"
    UI_CLASS_NAME "CockpitMockupUi"
    HEADER_INCLUDE "MockupUi.h")

set(MFD_MOCKUP_MINIMAL_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp
    ${MFD_MOCKUP_MINIMAL_GENERATED_SOURCE})

set(MFD_MOCKUP_MINIMAL_HEADERS
    ${MFD_MOCKUP_MINIMAL_GENERATED_HEADER})

add_executable(client_mockup_minimal
    ${MFD_MOCKUP_MINIMAL_HEADERS}
    ${MFD_MOCKUP_MINIMAL_SOURCES})

target_include_directories(client_mockup_minimal
    PRIVATE
        ${MFD_MOCKUP_MINIMAL_GENERATED_DIR})

target_link_libraries(client_mockup_minimal PRIVATE mfd::api mfd::client)

set_target_properties(client_mockup_minimal PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:client_mockup_minimal>")

add_custom_command(
    TARGET client_mockup_minimal
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${MFD_ROOT_DIR}/assets"
        "$<TARGET_FILE_DIR:client_mockup_minimal>/assets"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_RUNTIME_DLLS:client_mockup_minimal>
        "$<TARGET_FILE_DIR:client_mockup_minimal>"
    COMMAND_EXPAND_LISTS)

mfd_stage_runtime(client_mockup_minimal WITH_ASSETS WITH_RUNTIME_DLLS)
