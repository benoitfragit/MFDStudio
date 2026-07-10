# This file is part of MFDStudio.
# Project author: Benoit Fra
# Repository: https://github.com/benoitfragit/MFDStudio
#
# Main interactive HUD client. It owns the ImGui/DX11 shell, the operator
# panel and the mini-simulation, and consumes the reusable hud_runtime shared
# library for everything HUD-publishing related.
if(NOT WIN32)
    message(STATUS "Skipping hud_client because the interactive HUD requires Win32 + Dear ImGui + DX11.")
    return()
endif()

get_target_property(MFD_IMGUI_INCLUDE_DIRS imgui INTERFACE_INCLUDE_DIRECTORIES)
if(NOT MFD_IMGUI_INCLUDE_DIRS)
    message(FATAL_ERROR "Unable to resolve Dear ImGui source directories for hud_client.")
endif()
list(GET MFD_IMGUI_INCLUDE_DIRS 0 MFD_IMGUI_SOURCE_DIR)

set(MFD_HUD_MAIN_PLATFORM_HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/include/hud_main/Win32Dx11ImGuiHost.h)

set(MFD_HUD_MAIN_PLATFORM_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Win32Dx11ImGuiHost.cpp
    ${MFD_IMGUI_SOURCE_DIR}/backends/imgui_impl_dx11.cpp
    ${MFD_IMGUI_SOURCE_DIR}/backends/imgui_impl_win32.cpp)

set(MFD_HUD_MAIN_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/HudApplication.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/HudPhysics.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/HudSimulation.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp
    ${MFD_HUD_MAIN_PLATFORM_SOURCES})

set(MFD_HUD_MAIN_HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/include/hud_main/HudApplication.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/hud_main/HudPhysics.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/hud_main/HudSimulation.h
    ${MFD_HUD_MAIN_PLATFORM_HEADERS})

add_executable(hud_client
    ${MFD_HUD_MAIN_HEADERS}
    ${MFD_HUD_MAIN_SOURCES})

target_include_directories(hud_client
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_libraries(hud_client
    PRIVATE
        d3d11
        dxgi
        imgui
        hud_runtime)

mfd_enable_target_warnings(hud_client)

set_target_properties(hud_client PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:hud_client>")

# Stage the HUD runtime assets and every runtime DLL (hud_runtime.dll and the
# MFD client DLLs) next to the executable so it runs from its build directory.
mfd_stage_hud_runtime_assets(hud_client)

add_custom_command(
    TARGET hud_client
    POST_BUILD
    COMMAND
        ${CMAKE_COMMAND}
        "-DDEST_DIR=$<TARGET_FILE_DIR:hud_client>"
        "-DRUNTIME_DLLS=$<JOIN:$<TARGET_RUNTIME_DLLS:hud_client>,$<SEMICOLON>>"
        -P
        "${MFD_ROOT_DIR}/cmake/CopyRuntimeDependencies.cmake"
    VERBATIM)

mfd_stage_runtime(
    hud_client
    ROOT_ASSET_DIRS "${MFD_HUD_RUNTIME_ASSET_DIR}"
    WITH_RUNTIME_DLLS
    SCRIPT_SOURCE_DIR "${MFD_ROOT_DIR}/Scripts"
    SCRIPT_NAMES Start-HUD.bat Start-MfdWindow.bat)

if(MFD_BUILD_TESTS)
    # The smoke test checks the runtime DLL set of hud_client, which includes
    # hud_runtime.dll, next to the executable plus the staged HUD assets.
    mfd_add_runtime_layout_smoke_test(
        hud_client
        VERIFY_ASSETS
        ASSET_SOURCE_DIR "${MFD_HUD_RUNTIME_ASSET_DIR}"
        EXTRA_FILES
            assets/fonts/ShareTechMono-Regular.ttf
            assets/fonts/OFL-ShareTechMono.txt
            assets/windows/hud_window.json
        LABELS examples)
endif()
