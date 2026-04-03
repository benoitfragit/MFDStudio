# This file is part of MFDStudio.
# Project author: Benoit Fra
# Repository: https://github.com/benoitfragit/MFDStudio
set(MFD_DEMO_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp)

add_executable(mfd_demo ${MFD_DEMO_SOURCES})

target_link_libraries(mfd_demo
    PRIVATE
        mfd::window_launcher)

set_target_properties(mfd_demo PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:mfd_demo>")

add_custom_command(
    TARGET mfd_demo
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${MFD_ROOT_DIR}/assets"
        "$<TARGET_FILE_DIR:mfd_demo>/assets"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_RUNTIME_DLLS:mfd_demo>
        "$<TARGET_FILE_DIR:mfd_demo>"
    COMMAND_EXPAND_LISTS)

mfd_stage_runtime(mfd_demo WITH_ASSETS WITH_RUNTIME_DLLS)
