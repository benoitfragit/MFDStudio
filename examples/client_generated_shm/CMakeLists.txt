# This file is part of MFDStudio.
# Project author: Benoit Fra
# Repository: https://github.com/benoitfragit/MFDStudio
set(MFD_CLIENT_GENERATED_SHM_DIR
    ${CMAKE_CURRENT_SOURCE_DIR}/generated)

set(MFD_CLIENT_GENERATED_UI_HEADER
    ${MFD_CLIENT_GENERATED_SHM_DIR}/CockpitUiGenerated.h)
set(MFD_CLIENT_GENERATED_UI_SOURCE
    ${MFD_CLIENT_GENERATED_SHM_DIR}/CockpitUiGenerated.cpp)
set(MFD_CLIENT_GENERATED_SHM_HEADER
    ${MFD_CLIENT_GENERATED_SHM_DIR}/CockpitShmGenerated.h)
set(MFD_CLIENT_GENERATED_SHM_SOURCE
    ${MFD_CLIENT_GENERATED_SHM_DIR}/CockpitShmGenerated.cpp)

client_api_generate_ui(
    WINDOW_JSON "assets/windows/demo_pages_cockpit_shm.json"
    OUTPUT_HEADER "${MFD_CLIENT_GENERATED_UI_HEADER}"
    OUTPUT_SOURCE "${MFD_CLIENT_GENERATED_UI_SOURCE}"
    OUTPUT_SHM_HEADER "${MFD_CLIENT_GENERATED_SHM_HEADER}"
    OUTPUT_SHM_SOURCE "${MFD_CLIENT_GENERATED_SHM_SOURCE}"
    NAMESPACE "cockpit_shm_generated"
    UI_CLASS_NAME "CockpitShmUi"
    HEADER_INCLUDE "CockpitUiGenerated.h")

add_executable(client_generated_shm
    ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp
    ${MFD_CLIENT_GENERATED_UI_SOURCE}
    ${MFD_CLIENT_GENERATED_SHM_SOURCE}
    ${MFD_CLIENT_GENERATED_UI_HEADER}
    ${MFD_CLIENT_GENERATED_SHM_HEADER})

target_include_directories(client_generated_shm
    PRIVATE
        ${MFD_CLIENT_GENERATED_SHM_DIR})

target_link_libraries(client_generated_shm
    PRIVATE
        mfd::api
        mfd::client)

add_library(generated_cockpit_shm_plugin SHARED
    ${MFD_CLIENT_GENERATED_SHM_SOURCE}
    ${MFD_CLIENT_GENERATED_SHM_HEADER})

target_include_directories(generated_cockpit_shm_plugin
    PRIVATE
        ${MFD_CLIENT_GENERATED_SHM_DIR})

target_link_libraries(generated_cockpit_shm_plugin
    PRIVATE
        mfd::api)

set_target_properties(client_generated_shm PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:client_generated_shm>")

add_custom_command(
    TARGET client_generated_shm
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${MFD_ROOT_DIR}/assets"
        "$<TARGET_FILE_DIR:client_generated_shm>/assets"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:generated_cockpit_shm_plugin>
        "$<TARGET_FILE_DIR:client_generated_shm>"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:generated_cockpit_shm_plugin>
        "$<TARGET_FILE_DIR:mfd_demo_cockpit_shm>"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_RUNTIME_DLLS:client_generated_shm>
        "$<TARGET_FILE_DIR:client_generated_shm>"
    COMMAND_EXPAND_LISTS)

mfd_stage_runtime(client_generated_shm WITH_ASSETS WITH_RUNTIME_DLLS)
mfd_stage_runtime(generated_cockpit_shm_plugin WITH_RUNTIME_DLLS)
