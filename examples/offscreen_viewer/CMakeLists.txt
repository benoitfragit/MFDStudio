# This file is part of MFDStudio.
# Project author: Benoit Fra
# Repository: https://github.com/benoitfragit/MFDStudio
add_executable(offscreen_viewer
    ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp)

set(MFD_OFFSCREEN_VIEWER_ASSET_DIR "${MFD_ROOT_DIR}/examples/demo/assets")

target_link_libraries(offscreen_viewer
    PRIVATE
        mfd::runtime_api
        raylib)

target_compile_features(offscreen_viewer PRIVATE cxx_std_17)

set_target_properties(offscreen_viewer PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:offscreen_viewer>")

add_custom_command(
    TARGET offscreen_viewer
    POST_BUILD
    COMMAND
        ${CMAKE_COMMAND}
        "-DSOURCE_DIR=${MFD_OFFSCREEN_VIEWER_ASSET_DIR}"
        "-DDEST_DIR=$<TARGET_FILE_DIR:offscreen_viewer>/assets"
        -P
        "${MFD_ROOT_DIR}/cmake/SyncAssetTree.cmake"
    COMMAND
        ${CMAKE_COMMAND}
        "-DDEST_DIR=$<TARGET_FILE_DIR:offscreen_viewer>"
        "-DRUNTIME_DLLS=$<JOIN:$<TARGET_RUNTIME_DLLS:offscreen_viewer>,$<SEMICOLON>>"
        -P
        "${MFD_ROOT_DIR}/cmake/CopyRuntimeDependencies.cmake"
    VERBATIM)

mfd_stage_runtime(
    offscreen_viewer
    WITH_RUNTIME_DLLS
    ROOT_ASSET_DIRS "${MFD_OFFSCREEN_VIEWER_ASSET_DIR}")

if(MFD_BUILD_TESTS)
    mfd_add_runtime_layout_smoke_test(
        offscreen_viewer
        VERIFY_ASSETS
        ASSET_SOURCE_DIR "${MFD_OFFSCREEN_VIEWER_ASSET_DIR}"
        LABELS examples)
endif()
