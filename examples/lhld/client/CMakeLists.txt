# This file is part of MFDStudio.
# Project author: Benoit Fra
# Repository: https://github.com/benoitfragit/MFDStudio
if(NOT WIN32)
    message(STATUS "Skipping lhld_client because the interactive MFD demo requires Win32 + Dear ImGui + DX11.")
    return()
endif()

set(MFD_LHLD_ASSET_DIR
    "${CMAKE_CURRENT_SOURCE_DIR}/../assets")

set(MFD_LHLD_GENERATED_DIR
    ${CMAKE_CURRENT_SOURCE_DIR}/generated)

set(MFD_LHLD_GENERATED_HEADER
    ${MFD_LHLD_GENERATED_DIR}/LhldUi.h)

set(MFD_LHLD_GENERATED_SOURCE
    ${MFD_LHLD_GENERATED_DIR}/LhldUi.cpp)

client_api_generate_ui(
    WINDOW_JSON "${MFD_LHLD_ASSET_DIR}/windows/lhld_window.json"
    OUTPUT_HEADER "${MFD_LHLD_GENERATED_HEADER}"
    OUTPUT_SOURCE "${MFD_LHLD_GENERATED_SOURCE}"
    OUTPUT_MAP "${MFD_LHLD_ASSET_DIR}/windows/lhld_window.generated.map"
    NAMESPACE "lhld_ui"
    UI_CLASS_NAME "LhldUi"
    HEADER_INCLUDE "LhldUi.h")

get_target_property(MFD_IMGUI_INCLUDE_DIRS imgui INTERFACE_INCLUDE_DIRECTORIES)
if(NOT MFD_IMGUI_INCLUDE_DIRS)
    message(FATAL_ERROR "Unable to resolve Dear ImGui source directories for lhld_client.")
endif()
list(GET MFD_IMGUI_INCLUDE_DIRS 0 MFD_IMGUI_SOURCE_DIR)

set(MFD_LHLD_PLATFORM_HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Win32Dx11ImGuiHost.h)

set(MFD_LHLD_PLATFORM_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Win32Dx11ImGuiHost.cpp
    ${MFD_IMGUI_SOURCE_DIR}/backends/imgui_impl_dx11.cpp
    ${MFD_IMGUI_SOURCE_DIR}/backends/imgui_impl_win32.cpp)

set(MFD_LHLD_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/CockpitUi.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Dx11TextureUploader.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/MfdApplication.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/MfdController.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/MfdProjection.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/MfdRadarGeometry.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/MfdRadarSimulation.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/OffscreenMfdView.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp
    ${MFD_LHLD_GENERATED_SOURCE}
    ${MFD_LHLD_PLATFORM_SOURCES})

set(MFD_LHLD_HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/CockpitUi.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Dx11TextureUploader.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/MfdApplication.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/MfdController.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/MfdInputSource.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/MfdProjection.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/MfdRadarGeometry.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/MfdRadarSimulation.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/MfdTypes.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/OffscreenMfdView.h
    ${MFD_LHLD_GENERATED_HEADER}
    ${MFD_LHLD_PLATFORM_HEADERS})

add_executable(lhld_client
    ${MFD_LHLD_HEADERS}
    ${MFD_LHLD_SOURCES})

target_include_directories(lhld_client
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${MFD_LHLD_GENERATED_DIR})

target_link_libraries(lhld_client
    PRIVATE
        d3d11
        dxgi
        imgui
        raylib
        mfd::client
        mfd::runtime_api)

mfd_enable_target_warnings(lhld_client)

set_target_properties(lhld_client PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:lhld_client>")

add_custom_command(
    TARGET lhld_client
    POST_BUILD
    COMMAND
        ${CMAKE_COMMAND}
        "-DSOURCE_DIR=${MFD_LHLD_ASSET_DIR}"
        "-DDEST_DIR=$<TARGET_FILE_DIR:lhld_client>/assets"
        -P
        "${MFD_ROOT_DIR}/cmake/SyncAssetTree.cmake"
    COMMAND
        ${CMAKE_COMMAND}
        "-DDEST_DIR=$<TARGET_FILE_DIR:lhld_client>"
        "-DRUNTIME_DLLS=$<JOIN:$<TARGET_RUNTIME_DLLS:lhld_client>,$<SEMICOLON>>"
        -P
        "${MFD_ROOT_DIR}/cmake/CopyRuntimeDependencies.cmake"
    VERBATIM)

mfd_stage_runtime(
    lhld_client
    ROOT_ASSET_DIRS "${MFD_LHLD_ASSET_DIR}"
    WITH_RUNTIME_DLLS
    SCRIPT_SOURCE_DIR "${MFD_ROOT_DIR}/Scripts"
    SCRIPT_NAMES Start-LHLD.bat)

if(MFD_BUILD_TESTS)
    mfd_add_runtime_layout_smoke_test(
        lhld_client
        VERIFY_ASSETS
        ASSET_SOURCE_DIR "${MFD_LHLD_ASSET_DIR}"
        EXTRA_FILES
            assets/fonts/ShareTechMono-Regular.ttf
            assets/fonts/OFL-ShareTechMono.txt
            assets/windows/lhld_window.json
        LABELS examples)
endif()
