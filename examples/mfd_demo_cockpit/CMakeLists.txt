# This file is part of MFDStudio.
# Project author: Benoit Fra
# Repository: https://github.com/benoitfragit/MFDStudio
set(MFD_DEMO_COCKPIT_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp)

add_executable(mfd_demo_cockpit ${MFD_DEMO_COCKPIT_SOURCES})

target_link_libraries(mfd_demo_cockpit
    PRIVATE
        mfd::window_launcher)

set_target_properties(mfd_demo_cockpit PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:mfd_demo_cockpit>")

add_custom_command(
    TARGET mfd_demo_cockpit
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${MFD_ROOT_DIR}/assets"
        "$<TARGET_FILE_DIR:mfd_demo_cockpit>/assets"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_RUNTIME_DLLS:mfd_demo_cockpit>
        "$<TARGET_FILE_DIR:mfd_demo_cockpit>"
    COMMAND_EXPAND_LISTS)

mfd_stage_runtime(mfd_demo_cockpit WITH_ASSETS WITH_RUNTIME_DLLS)
