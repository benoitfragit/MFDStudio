# This file is part of MFDStudio.
# Project author: Benoit Fra
# Repository: https://github.com/benoitfragit/MFDStudio
if(NOT WIN32)
    message(STATUS "Skipping demo_hud_client because the interactive HUD demo requires Win32 + Dear ImGui + DX11.")
    return()
endif()

set(MFD_DEMO_HUD_ASSET_DIR
    "${CMAKE_CURRENT_SOURCE_DIR}/../assets")

set(MFD_DEMO_HUD_GENERATED_DIR
    ${CMAKE_CURRENT_SOURCE_DIR}/generated)

set(MFD_DEMO_HUD_GENERATED_HEADER
    ${MFD_DEMO_HUD_GENERATED_DIR}/DemoHudUi.h)

set(MFD_DEMO_HUD_GENERATED_SOURCE
    ${MFD_DEMO_HUD_GENERATED_DIR}/DemoHudUi.cpp)

client_api_generate_ui(
    WINDOW_JSON "${MFD_DEMO_HUD_ASSET_DIR}/windows/demo_hud_window.json"
    OUTPUT_HEADER "${MFD_DEMO_HUD_GENERATED_HEADER}"
    OUTPUT_SOURCE "${MFD_DEMO_HUD_GENERATED_SOURCE}"
    OUTPUT_MAP "${MFD_DEMO_HUD_ASSET_DIR}/windows/demo_hud_window.generated.map"
    NAMESPACE "demo_hud_ui"
    UI_CLASS_NAME "DemoHudUi"
    HEADER_INCLUDE "DemoHudUi.h")

get_target_property(MFD_IMGUI_INCLUDE_DIRS imgui INTERFACE_INCLUDE_DIRECTORIES)
if(NOT MFD_IMGUI_INCLUDE_DIRS)
    message(FATAL_ERROR "Unable to resolve Dear ImGui source directories for demo_hud_client.")
endif()
list(GET MFD_IMGUI_INCLUDE_DIRS 0 MFD_IMGUI_SOURCE_DIR)

set(MFD_DEMO_HUD_PLATFORM_HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Win32Dx11ImGuiHost.h)

set(MFD_DEMO_HUD_PLATFORM_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Win32Dx11ImGuiHost.cpp
    ${MFD_IMGUI_SOURCE_DIR}/backends/imgui_impl_dx11.cpp
    ${MFD_IMGUI_SOURCE_DIR}/backends/imgui_impl_win32.cpp)

set(MFD_DEMO_HUD_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/DemoHudApplication.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/DemoHudController.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/DemoHudProjection.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/DemoHudSimulation.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp
    ${MFD_DEMO_HUD_GENERATED_SOURCE}
    ${MFD_DEMO_HUD_PLATFORM_SOURCES})

set(MFD_DEMO_HUD_HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/DemoHudApplication.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/DemoHudController.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/DemoHudProjection.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/DemoHudSimulation.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/DemoHudTypes.h
    ${MFD_DEMO_HUD_GENERATED_HEADER}
    ${MFD_DEMO_HUD_PLATFORM_HEADERS})

add_executable(demo_hud_client
    ${MFD_DEMO_HUD_HEADERS}
    ${MFD_DEMO_HUD_SOURCES})

target_include_directories(demo_hud_client
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${MFD_DEMO_HUD_GENERATED_DIR})

target_link_libraries(demo_hud_client
    PRIVATE
        d3d11
        dxgi
        imgui
        mfd::client)

mfd_enable_target_warnings(demo_hud_client)

set_target_properties(demo_hud_client PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:demo_hud_client>")

add_custom_command(
    TARGET demo_hud_client
    POST_BUILD
    COMMAND
        ${CMAKE_COMMAND}
        "-DSOURCE_DIR=${MFD_DEMO_HUD_ASSET_DIR}"
        "-DDEST_DIR=$<TARGET_FILE_DIR:demo_hud_client>/assets"
        -P
        "${MFD_ROOT_DIR}/cmake/SyncAssetTree.cmake"
    COMMAND
        ${CMAKE_COMMAND}
        "-DDEST_DIR=$<TARGET_FILE_DIR:demo_hud_client>"
        "-DRUNTIME_DLLS=$<JOIN:$<TARGET_RUNTIME_DLLS:demo_hud_client>,$<SEMICOLON>>"
        -P
        "${MFD_ROOT_DIR}/cmake/CopyRuntimeDependencies.cmake"
    VERBATIM)

mfd_stage_runtime(
    demo_hud_client
    ROOT_ASSET_DIRS "${MFD_DEMO_HUD_ASSET_DIR}"
    WITH_RUNTIME_DLLS
    SCRIPT_SOURCE_DIR "${MFD_ROOT_DIR}/Scripts"
    SCRIPT_NAMES Start-DemoHUD.bat Start-MfdWindow.bat)

if(MFD_BUILD_TESTS)
    mfd_add_runtime_layout_smoke_test(
        demo_hud_client
        VERIFY_ASSETS
        ASSET_SOURCE_DIR "${MFD_DEMO_HUD_ASSET_DIR}"
        EXTRA_FILES
            assets/fonts/ShareTechMono-Regular.ttf
            assets/fonts/OFL-ShareTechMono.txt
            assets/windows/demo_hud_window.json
        LABELS examples)
endif()
