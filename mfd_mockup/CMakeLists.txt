# This file is part of MFDStudio.
# Project author: Benoit Fra
# Repository: https://github.com/benoitfragit/MFDStudio
set(MFD_MOCKUP_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp)

add_executable(mfd_mockup ${MFD_MOCKUP_SOURCES})

target_link_libraries(mfd_mockup
    PRIVATE
        mfd::api
        imgui
        rlimgui)

set_target_properties(mfd_mockup PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:mfd_mockup>")

add_custom_command(
    TARGET mfd_mockup
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${MFD_ROOT_DIR}/assets"
        "$<TARGET_FILE_DIR:mfd_mockup>/assets"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_RUNTIME_DLLS:mfd_mockup>
        "$<TARGET_FILE_DIR:mfd_mockup>"
    COMMAND_EXPAND_LISTS)

mfd_stage_runtime(mfd_mockup WITH_ASSETS WITH_RUNTIME_DLLS)
