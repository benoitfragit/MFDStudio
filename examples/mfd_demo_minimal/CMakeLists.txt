# This file is part of MFDStudio.
# Project author: Benoit Fra
# Repository: https://github.com/benoitfragit/MFDStudio
set(MFD_DEMO_MINIMAL_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp)

add_executable(mfd_demo_minimal ${MFD_DEMO_MINIMAL_SOURCES})

target_link_libraries(mfd_demo_minimal PRIVATE mfd::window_launcher)

set_target_properties(mfd_demo_minimal PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:mfd_demo_minimal>")

add_custom_command(
    TARGET mfd_demo_minimal
    POST_BUILD
    COMMAND
        ${CMAKE_COMMAND}
        "-DSOURCE_DIR=${MFD_ROOT_DIR}/assets"
        "-DDEST_DIR=$<TARGET_FILE_DIR:mfd_demo_minimal>/assets"
        -P
        "${MFD_ROOT_DIR}/cmake/SyncAssetTree.cmake"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_RUNTIME_DLLS:mfd_demo_minimal>
        $<TARGET_RUNTIME_DLLS:mfd_api>
        "$<TARGET_FILE_DIR:mfd_demo_minimal>"
    COMMAND_EXPAND_LISTS)

mfd_stage_runtime(mfd_demo_minimal WITH_ASSETS WITH_RUNTIME_DLLS)
