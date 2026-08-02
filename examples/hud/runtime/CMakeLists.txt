# This file is part of MFDStudio.
# Project author: Benoit Fra
# Repository: https://github.com/benoitfragit/MFDStudio
#
# Reusable HUD runtime shared library. It contains everything an external
# aircraft simulation needs to publish HUD frames from a `hud::HudInputSample`:
# the generated HUD UI, the projection/symbology geometry, the generated-UI
# adapter and the transport lifecycle behind `hud::HudRuntimeClient`. It must
# stay free of ImGui, Win32, DX11 and main-client constants.
set(MFD_HUD_ASSET_DIR
    "${CMAKE_CURRENT_SOURCE_DIR}/../assets")

# Published for consumers (hud_client today, external executables tomorrow)
# that stage the HUD runtime assets through mfd_stage_hud_runtime_assets().
set(MFD_HUD_RUNTIME_ASSET_DIR
    "${MFD_HUD_ASSET_DIR}"
    CACHE INTERNAL
    "Source directory of the HUD runtime assets staged next to consumers.")

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

set(MFD_HUD_RUNTIME_PUBLIC_HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/include/hud/HudController.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/hud/HudProjection.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/hud/HudRuntimeClient.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/hud/HudRuntimeExport.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/hud/HudTypes.h)

add_library(hud_runtime SHARED
    ${MFD_HUD_RUNTIME_PUBLIC_HEADERS}
    ${CMAKE_CURRENT_SOURCE_DIR}/src/HudLayout.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/HudController.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/HudProjection.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/HudRuntimeClient.cpp
    ${MFD_HUD_GENERATED_HEADER}
    ${MFD_HUD_GENERATED_SOURCE})

target_compile_definitions(hud_runtime
    PRIVATE
        HUD_RUNTIME_BUILDING_DLL)

target_include_directories(hud_runtime
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${MFD_HUD_GENERATED_DIR})

# mfd::client is an implementation detail: HudRuntimeClient hides every mfd type
# behind its PIMPL and no public runtime header includes an mfd header, so the
# dependency stays PRIVATE. hud_runtime is a shared library, so mfd_client_api
# remains in the transitive runtime-DLL closure of consumers such as hud_client.
target_link_libraries(hud_runtime
    PRIVATE
        mfd::client)

mfd_enable_target_warnings(hud_runtime)

# Stages every asset the HUD runtime needs at execution time (window JSON with
# its generated transport map, pages, reticles and the HUD font) next to the
# consumer executable. Reusable by any target linking hud_runtime.
function(mfd_stage_hud_runtime_assets target_name)
    add_custom_command(
        TARGET ${target_name}
        POST_BUILD
        COMMAND
            ${CMAKE_COMMAND}
            "-DSOURCE_DIR=${MFD_HUD_RUNTIME_ASSET_DIR}"
            "-DDEST_DIR=$<TARGET_FILE_DIR:${target_name}>/assets"
            -P
            "${MFD_ROOT_DIR}/cmake/SyncAssetTree.cmake"
        VERBATIM)
endfunction()
