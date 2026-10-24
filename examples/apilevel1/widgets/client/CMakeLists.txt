# This file is part of MFDStudio.
# Project author: Benoit Fra
# Repository: https://github.com/benoitfragit/MFDStudio

set(MFD_WIDGETS_EXAMPLE_ASSET_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../assets")
set(MFD_WIDGETS_EXAMPLE_GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")

client_api_generate_ui(
    WINDOW_JSON "${MFD_WIDGETS_EXAMPLE_ASSET_DIR}/windows/widgets_window.json"
    OUTPUT_HEADER "${MFD_WIDGETS_EXAMPLE_GENERATED_DIR}/WidgetsExampleUi.h"
    OUTPUT_SOURCE "${MFD_WIDGETS_EXAMPLE_GENERATED_DIR}/WidgetsExampleUi.cpp"
    OUTPUT_MAP "${MFD_WIDGETS_EXAMPLE_ASSET_DIR}/windows/widgets_window.generated.map"
    NAMESPACE "widgets_example"
    UI_CLASS_NAME "WidgetsExampleUi"
    HEADER_INCLUDE "WidgetsExampleUi.h")

add_executable(apilevel1_widgets_client
    ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/CockpitDemoController.h
    ${CMAKE_CURRENT_SOURCE_DIR}/CockpitDemoController.cpp
    ${MFD_WIDGETS_EXAMPLE_GENERATED_DIR}/WidgetsExampleUi.h
    ${MFD_WIDGETS_EXAMPLE_GENERATED_DIR}/WidgetsExampleUi.cpp)
target_include_directories(apilevel1_widgets_client PRIVATE ${MFD_WIDGETS_EXAMPLE_GENERATED_DIR})
target_link_libraries(apilevel1_widgets_client PRIVATE mfd::client mfd::io_json)
mfd_enable_target_warnings(apilevel1_widgets_client)

add_custom_command(
    TARGET apilevel1_widgets_client POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${MFD_WIDGETS_EXAMPLE_ASSET_DIR}"
            "$<TARGET_FILE_DIR:apilevel1_widgets_client>/assets"
    COMMAND ${CMAKE_COMMAND}
            "-DDEST_DIR=$<TARGET_FILE_DIR:apilevel1_widgets_client>"
            "-DRUNTIME_DLLS=$<JOIN:$<TARGET_RUNTIME_DLLS:apilevel1_widgets_client>,$<SEMICOLON>>"
            -P "${MFD_ROOT_DIR}/cmake/CopyRuntimeDependencies.cmake"
    VERBATIM)

mfd_stage_runtime(
    apilevel1_widgets_client
    ROOT_ASSET_DIRS "${MFD_WIDGETS_EXAMPLE_ASSET_DIR}"
    SCRIPT_SOURCE_DIR "${MFD_ROOT_DIR}/Scripts"
    SCRIPT_NAMES Start-APILevel1Widgets.bat Start-MfdWindow.bat
    WITH_RUNTIME_DLLS)


if(MFD_BUILD_TESTS)
    add_executable(apilevel1_widgets_client_tests WidgetExampleTests.cpp
        CockpitDemoController.h
        CockpitDemoController.cpp
        ${MFD_WIDGETS_EXAMPLE_GENERATED_DIR}/WidgetsExampleUi.cpp)
    target_include_directories(apilevel1_widgets_client_tests PRIVATE ${MFD_WIDGETS_EXAMPLE_GENERATED_DIR})
    target_compile_definitions(apilevel1_widgets_client_tests PRIVATE
        MFD_WIDGET_EXAMPLE_WINDOW="${MFD_WIDGETS_EXAMPLE_ASSET_DIR}/windows/widgets_window.json")
    target_link_libraries(apilevel1_widgets_client_tests PRIVATE mfd::client EnTT::EnTT GTest::gtest_main)
    mfd_configure_test_runtime_output(apilevel1_widgets_client_tests)
    if(WIN32)
        add_custom_command(TARGET apilevel1_widgets_client_tests POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:mfd_api> $<TARGET_FILE:mfd_client_api>
                $<TARGET_FILE_DIR:apilevel1_widgets_client_tests>/ VERBATIM)
    endif()
    include(GoogleTest)
    gtest_discover_tests(apilevel1_widgets_client_tests DISCOVERY_TIMEOUT 60)
    mfd_add_runtime_layout_smoke_test(
        apilevel1_widgets_client
        VERIFY_ASSETS
        ASSET_SOURCE_DIR "${MFD_WIDGETS_EXAMPLE_ASSET_DIR}"
        EXTRA_FILES assets/windows/widgets_window.json
        LABELS examples apilevel1)
endif()

if(MFD_BUILD_TESTS AND WIN32)
    add_executable(apilevel1_widget_capability_tests WidgetCapabilityTests.cpp
        ${MFD_WIDGETS_ROOT_DIR}/runtime/src/WidgetRuntimeHost.cpp)
    target_include_directories(apilevel1_widget_capability_tests PRIVATE ${MFD_WIDGETS_INCLUDE_DIR})
    target_compile_definitions(apilevel1_widget_capability_tests PRIVATE
        MFD_WIDGET_EXAMPLE_ASSETS="${MFD_WIDGETS_EXAMPLE_ASSET_DIR}"
        MFD_WIDGET_RUNTIME_FILE="$<TARGET_FILE:mfd_widgets_api1>")
    target_link_libraries(apilevel1_widget_capability_tests PRIVATE mfd::runtime_api nlohmann_json::nlohmann_json GTest::gtest_main)
    set_target_properties(apilevel1_widget_capability_tests PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/without_widget_extension")
    add_dependencies(apilevel1_widget_capability_tests apilevel1_widgets_client mfd_widgets_api1 mfd_containers_api1)
    add_custom_command(TARGET apilevel1_widget_capability_tests POST_BUILD
        COMMAND ${CMAKE_COMMAND}
            "-DDEST_DIR=$<TARGET_FILE_DIR:apilevel1_widget_capability_tests>"
            "-DRUNTIME_DLLS=$<JOIN:$<TARGET_RUNTIME_DLLS:apilevel1_widget_capability_tests>,$<SEMICOLON>>"
            -P "${MFD_ROOT_DIR}/cmake/CopyRuntimeDependencies.cmake"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:mfd_containers_api1>
            $<TARGET_FILE_DIR:apilevel1_widget_capability_tests>/ VERBATIM)
    gtest_discover_tests(apilevel1_widget_capability_tests
        WORKING_DIRECTORY "$<TARGET_FILE_DIR:apilevel1_widget_capability_tests>" DISCOVERY_TIMEOUT 60)
endif()
