# This file is part of MFDStudio.
# Project author: Benoit Fra
# Repository: https://github.com/benoitfragit/MFDStudio
if(NOT WIN32)
    message(STATUS "Skipping hud_client because the interactive HUD requires Win32 + Dear ImGui + DX11.")
    return()
endif()

set(MFD_HUD_ASSET_DIR
    "${CMAKE_CURRENT_SOURCE_DIR}/../assets")

set(MFD_HUD_GENERATED_DIR
    ${CMAKE_CURRENT_SOURCE_DIR}/generated)

set(MFD_HUD_GENERATED_HEADER
    ${MFD_HUD_GENERATED_DIR}/HudUi.h)

set(MFD_HUD_GENERATED_SOURCE
    ${MFD_HUD_GENERATED_DIR}/HudUi.cpp)

client_api_generate_ui(
    WINDOW_JSON "${MFD_HUD_ASSET_DIR}/windows/hud_window.json"
    OUTPUT_HEADER "${MFD_HUD_GENERATED_HEADER}"
    OUTPUT_SOURCE "${MFD_HUD_GENERATED_SOURCE}"
    OUTPUT_MAP "${MFD_HUD_ASSET_DIR}/windows/hud_window.generated.map"
    NAMESPACE "hud_ui"
    UI_CLASS_NAME "HudUi"
    HEADER_INCLUDE "HudUi.h")

get_target_property(MFD_IMGUI_INCLUDE_DIRS imgui INTERFACE_INCLUDE_DIRECTORIES)
if(NOT MFD_IMGUI_INCLUDE_DIRS)
    message(FATAL_ERROR "Unable to resolve Dear ImGui source directories for hud_client.")
endif()
list(GET MFD_IMGUI_INCLUDE_DIRS 0 MFD_IMGUI_SOURCE_DIR)

set(MFD_HUD_PLATFORM_HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Win32Dx11ImGuiHost.h)

set(MFD_HUD_PLATFORM_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Win32Dx11ImGuiHost.cpp
    ${MFD_IMGUI_SOURCE_DIR}/backends/imgui_impl_dx11.cpp
    ${MFD_IMGUI_SOURCE_DIR}/backends/imgui_impl_win32.cpp)

set(MFD_HUD_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/HudApplication.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/HudController.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/HudProjection.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/HudSimulation.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp
    ${MFD_HUD_GENERATED_SOURCE}
    ${MFD_HUD_PLATFORM_SOURCES})

set(MFD_HUD_HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/HudApplication.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/HudController.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/HudProjection.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/HudSimulation.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/HudTypes.h
    ${MFD_HUD_GENERATED_HEADER}
    ${MFD_HUD_PLATFORM_HEADERS})

add_executable(hud_client
    ${MFD_HUD_HEADERS}
    ${MFD_HUD_SOURCES})

target_include_directories(hud_client
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${MFD_HUD_GENERATED_DIR})

target_link_libraries(hud_client
    PRIVATE
        d3d11
        dxgi
        imgui
        mfd::client)

mfd_enable_target_warnings(hud_client)

set_target_properties(hud_client PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:hud_client>")

add_custom_command(
    TARGET hud_client
    POST_BUILD
    COMMAND
        ${CMAKE_COMMAND}
        "-DSOURCE_DIR=${MFD_HUD_ASSET_DIR}"
        "-DDEST_DIR=$<TARGET_FILE_DIR:hud_client>/assets"
        -P
        "${MFD_ROOT_DIR}/cmake/SyncAssetTree.cmake"
    COMMAND
        ${CMAKE_COMMAND}
        "-DDEST_DIR=$<TARGET_FILE_DIR:hud_client>"
        "-DRUNTIME_DLLS=$<JOIN:$<TARGET_RUNTIME_DLLS:hud_client>,$<SEMICOLON>>"
        -P
        "${MFD_ROOT_DIR}/cmake/CopyRuntimeDependencies.cmake"
    VERBATIM)

mfd_stage_runtime(
    hud_client
    ROOT_ASSET_DIRS "${MFD_HUD_ASSET_DIR}"
    WITH_RUNTIME_DLLS
    SCRIPT_SOURCE_DIR "${MFD_ROOT_DIR}/Scripts"
    SCRIPT_NAMES Start-HUD.bat Start-MfdWindow.bat)

if(MFD_BUILD_TESTS)
    mfd_add_runtime_layout_smoke_test(
        hud_client
        VERIFY_ASSETS
        ASSET_SOURCE_DIR "${MFD_HUD_ASSET_DIR}"
        EXTRA_FILES
            assets/fonts/ShareTechMono-Regular.ttf
            assets/fonts/OFL-ShareTechMono.txt
            assets/windows/hud_window.json
        LABELS examples)
endif()
