# This file is part of MFDStudio.
# Project author: Benoit Fra
# Repository: https://github.com/benoitfragit/MFDStudio
if(NOT WIN32)
    message(STATUS "Skipping demo_client because the interactive demo client requires Win32 + Dear ImGui + DX11.")
    return()
endif()

set(MFD_DEMO_ASSET_DIR
    "${CMAKE_CURRENT_SOURCE_DIR}/../assets")

set(MFD_DEMO_GENERATED_DIR
    ${CMAKE_CURRENT_SOURCE_DIR}/generated)

set(MFD_DEMO_GENERATED_HEADER
    ${MFD_DEMO_GENERATED_DIR}/DemoUi.h)

set(MFD_DEMO_GENERATED_SOURCE
    ${MFD_DEMO_GENERATED_DIR}/DemoUi.cpp)

client_api_generate_ui(
    WINDOW_JSON "${MFD_DEMO_ASSET_DIR}/windows/demo_window.json"
    OUTPUT_HEADER "${MFD_DEMO_GENERATED_HEADER}"
    OUTPUT_SOURCE "${MFD_DEMO_GENERATED_SOURCE}"
    OUTPUT_MAP "${MFD_DEMO_ASSET_DIR}/windows/demo_window.generated.map"
    NAMESPACE "demo_ui"
    UI_CLASS_NAME "DemoUi"
    HEADER_INCLUDE "DemoUi.h")

get_target_property(MFD_IMGUI_INCLUDE_DIRS imgui INTERFACE_INCLUDE_DIRECTORIES)
if(NOT MFD_IMGUI_INCLUDE_DIRS)
    message(FATAL_ERROR "Unable to resolve Dear ImGui source directories for demo_client.")
endif()
list(GET MFD_IMGUI_INCLUDE_DIRS 0 MFD_IMGUI_SOURCE_DIR)

set(MFD_DEMO_PLATFORM_HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Win32Dx11ImGuiHost.h)

set(MFD_DEMO_PLATFORM_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Win32Dx11ImGuiHost.cpp
    ${MFD_IMGUI_SOURCE_DIR}/backends/imgui_impl_dx11.cpp
    ${MFD_IMGUI_SOURCE_DIR}/backends/imgui_impl_win32.cpp)

add_executable(demo_client
    ${MFD_DEMO_GENERATED_HEADER}
    ${MFD_DEMO_PLATFORM_HEADERS}
    ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp
    ${MFD_DEMO_GENERATED_SOURCE}
    ${MFD_DEMO_PLATFORM_SOURCES})

target_include_directories(demo_client
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${MFD_DEMO_GENERATED_DIR})

target_link_libraries(demo_client
    PRIVATE
        d3d11
        dxgi
        imgui
        mfd::client)

mfd_enable_target_warnings(demo_client)

set_target_properties(demo_client PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:demo_client>")

add_custom_command(
    TARGET demo_client
    POST_BUILD
    COMMAND
        ${CMAKE_COMMAND}
        "-DSOURCE_DIR=${MFD_DEMO_ASSET_DIR}"
        "-DDEST_DIR=$<TARGET_FILE_DIR:demo_client>/assets"
        -P
        "${MFD_ROOT_DIR}/cmake/SyncAssetTree.cmake"
    COMMAND
        ${CMAKE_COMMAND}
        "-DDEST_DIR=$<TARGET_FILE_DIR:demo_client>"
        "-DRUNTIME_DLLS=$<JOIN:$<TARGET_RUNTIME_DLLS:demo_client>,$<SEMICOLON>>"
        -P
        "${MFD_ROOT_DIR}/cmake/CopyRuntimeDependencies.cmake"
    VERBATIM)

mfd_stage_runtime(
    demo_client
    ROOT_ASSET_DIRS "${MFD_DEMO_ASSET_DIR}"
    WITH_RUNTIME_DLLS
    SCRIPT_SOURCE_DIR "${MFD_ROOT_DIR}/Scripts"
    SCRIPT_NAMES Start-MfdDemo.bat Start-MfdWindow.bat)

if(MFD_BUILD_TESTS)
    mfd_add_runtime_layout_smoke_test(
        demo_client
        VERIFY_ASSETS
        ASSET_SOURCE_DIR "${MFD_DEMO_ASSET_DIR}"
        EXTRA_FILES
            assets/windows/demo_window.json
        LABELS examples)
endif()
