# This file is part of MFDStudio.
# Project author: Benoit Fra
# Repository: https://github.com/benoitfragit/MFDStudio
set(MFD_DEMO_COCKPIT_SHM_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp)

add_executable(mfd_demo_cockpit_shm ${MFD_DEMO_COCKPIT_SHM_SOURCES})

target_link_libraries(mfd_demo_cockpit_shm
    PRIVATE
        mfd::window_launcher)

set_target_properties(mfd_demo_cockpit_shm PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:mfd_demo_cockpit_shm>")

add_custom_command(
    TARGET mfd_demo_cockpit_shm
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${MFD_ROOT_DIR}/assets"
        "$<TARGET_FILE_DIR:mfd_demo_cockpit_shm>/assets"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_RUNTIME_DLLS:mfd_demo_cockpit_shm>
        "$<TARGET_FILE_DIR:mfd_demo_cockpit_shm>"
    COMMAND_EXPAND_LISTS)

mfd_stage_runtime(mfd_demo_cockpit_shm WITH_ASSETS WITH_RUNTIME_DLLS)
