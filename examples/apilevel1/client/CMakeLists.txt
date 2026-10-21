# This file is part of MFDStudio.
# Project author: Benoit Fra
# Repository: https://github.com/benoitfragit/MFDStudio

set(MFD_APILEVEL1_EXAMPLE_ASSET_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../assets")
set(MFD_APILEVEL1_EXAMPLE_GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")

client_api_generate_ui(
    WINDOW_JSON "${MFD_APILEVEL1_EXAMPLE_ASSET_DIR}/windows/containers_window.json"
    OUTPUT_HEADER "${MFD_APILEVEL1_EXAMPLE_GENERATED_DIR}/ContainersExampleUi.h"
    OUTPUT_SOURCE "${MFD_APILEVEL1_EXAMPLE_GENERATED_DIR}/ContainersExampleUi.cpp"
    OUTPUT_MAP "${MFD_APILEVEL1_EXAMPLE_ASSET_DIR}/windows/containers_window.generated.map"
    NAMESPACE "containers_example"
    UI_CLASS_NAME "ContainersExampleUi"
    HEADER_INCLUDE "ContainersExampleUi.h")

add_executable(apilevel1_containers_client
    ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
    ${MFD_APILEVEL1_EXAMPLE_GENERATED_DIR}/ContainersExampleUi.h
    ${MFD_APILEVEL1_EXAMPLE_GENERATED_DIR}/ContainersExampleUi.cpp)
target_include_directories(apilevel1_containers_client PRIVATE ${MFD_APILEVEL1_EXAMPLE_GENERATED_DIR})
target_link_libraries(apilevel1_containers_client PRIVATE mfd::client mfd::io_json)
mfd_enable_target_warnings(apilevel1_containers_client)

add_custom_command(
    TARGET apilevel1_containers_client POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${MFD_APILEVEL1_EXAMPLE_ASSET_DIR}"
            "$<TARGET_FILE_DIR:apilevel1_containers_client>/assets"
    COMMAND ${CMAKE_COMMAND}
            "-DDEST_DIR=$<TARGET_FILE_DIR:apilevel1_containers_client>"
            "-DRUNTIME_DLLS=$<JOIN:$<TARGET_RUNTIME_DLLS:apilevel1_containers_client>,$<SEMICOLON>>"
            -P "${MFD_ROOT_DIR}/cmake/CopyRuntimeDependencies.cmake"
    VERBATIM)

mfd_stage_runtime(
    apilevel1_containers_client
    ROOT_ASSET_DIRS "${MFD_APILEVEL1_EXAMPLE_ASSET_DIR}"
    SCRIPT_SOURCE_DIR "${MFD_ROOT_DIR}/Scripts"
    SCRIPT_NAMES Start-APILevel1Containers.bat Start-MfdWindow.bat
    WITH_RUNTIME_DLLS)

if(MFD_BUILD_TESTS)
    mfd_add_runtime_layout_smoke_test(
        apilevel1_containers_client
        VERIFY_ASSETS
        ASSET_SOURCE_DIR "${MFD_APILEVEL1_EXAMPLE_ASSET_DIR}"
        EXTRA_FILES assets/windows/containers_window.json
        LABELS examples apilevel1)
endif()
