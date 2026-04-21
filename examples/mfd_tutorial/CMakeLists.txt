# This file is part of MFDStudio.
# Project author: Benoit Fra
# Repository: https://github.com/benoitfragit/MFDStudio
set(MFD_TUTORIAL_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp)

add_executable(mfd_tutorial ${MFD_TUTORIAL_SOURCES})

target_link_libraries(mfd_tutorial PRIVATE mfd::window_launcher)

set_target_properties(mfd_tutorial PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:mfd_tutorial>")

add_custom_command(
    TARGET mfd_tutorial
    POST_BUILD
    COMMAND
        ${CMAKE_COMMAND}
        "-DSOURCE_DIR=${MFD_ROOT_DIR}/assets"
        "-DDEST_DIR=$<TARGET_FILE_DIR:mfd_tutorial>/assets"
        -P
        "${MFD_ROOT_DIR}/cmake/SyncAssetTree.cmake"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_RUNTIME_DLLS:mfd_tutorial>
        $<TARGET_RUNTIME_DLLS:mfd_api>
        "$<TARGET_FILE_DIR:mfd_tutorial>"
    COMMAND_EXPAND_LISTS)

mfd_stage_runtime(mfd_tutorial WITH_ASSETS WITH_RUNTIME_DLLS)
