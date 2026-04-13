# This file is part of MFDStudio.
# Project author: Benoit Fra
# Repository: https://github.com/benoitfragit/MFDStudio
set(MFD_CLIENT_TUTORIAL_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp)

add_executable(client_tutorial ${MFD_CLIENT_TUTORIAL_SOURCES})

target_link_libraries(client_tutorial PRIVATE mfd::api mfd::client)

set_target_properties(client_tutorial PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:client_tutorial>")

add_custom_command(
    TARGET client_tutorial
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${MFD_ROOT_DIR}/assets"
        "$<TARGET_FILE_DIR:client_tutorial>/assets"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_RUNTIME_DLLS:client_tutorial>
        "$<TARGET_FILE_DIR:client_tutorial>"
    COMMAND_EXPAND_LISTS)

mfd_stage_runtime(client_tutorial WITH_ASSETS WITH_RUNTIME_DLLS)
