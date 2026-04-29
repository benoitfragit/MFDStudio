# This file is part of MFDStudio.
# Project author: Benoit Fra
# Repository: https://github.com/benoitfragit/MFDStudio
include(GoogleTest)

function(mfd_add_editor_test_plugin target output_name source_file)
    add_library(${target} SHARED ${source_file})

    target_link_libraries(${target}
        PRIVATE
            mfd::editor_plugin_api)

    target_compile_features(${target} PRIVATE cxx_std_17)
    target_compile_definitions(${target} PRIVATE MFD_EDITOR_AUTOMATION_PLUGIN_EXPORTS=1)

    if(MFD_ENABLE_WARNINGS)
        if(MSVC)
            target_compile_options(${target}
                PRIVATE
                    /W4
                    /permissive-
                    /Zc:__cplusplus
                    /EHsc
                    /wd4251
                    /wd4275)
        else()
            target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
        endif()
    endif()

    set_target_properties(${target} PROPERTIES
        OUTPUT_NAME "${output_name}"
        VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:${target}>")

    mfd_stage_runtime(${target} STAGE_SUBDIR tests)
endfunction()

set(MFD_EDITOR_TEST_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/EditorAutomationTests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/EditorAutomationPluginLoaderTests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/EditorAutomationTestUtils.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/EditorDocumentSerializerTests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/EditorTutorialDataTests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/EditorUiThemeTests.cpp)

add_executable(mfd_editor_tests ${MFD_EDITOR_TEST_SOURCES})

mfd_configure_test_runtime_output(mfd_editor_tests)

set(MFD_EDITOR_TEST_AUTOMATION_PLUGIN_TARGETS
    mfd_editor_test_automation_plugin
    mfd_editor_test_extended_automation_plugin
    mfd_editor_test_failing_plugin
    mfd_editor_test_bad_abi_plugin
    mfd_editor_test_start_failure_plugin
    mfd_editor_test_incomplete_plugin
    mfd_editor_test_rollback_on_unload_plugin
    mfd_editor_test_stateless_plugin
    mfd_editor_test_save_all_plugin)

mfd_add_editor_test_plugin(
    mfd_editor_test_automation_plugin
    "mfd_editor_test_automation_plugin"
    ${CMAKE_CURRENT_SOURCE_DIR}/plugins/TestAutomationPlugin.cpp)

mfd_add_editor_test_plugin(
    mfd_editor_test_extended_automation_plugin
    "mfd_editor_test_extended_automation_plugin"
    ${CMAKE_CURRENT_SOURCE_DIR}/plugins/ExtendedAutomationPlugin.cpp)

mfd_add_editor_test_plugin(
    mfd_editor_test_failing_plugin
    "mfd_editor_test_failing_plugin"
    ${CMAKE_CURRENT_SOURCE_DIR}/plugins/FailingAutomationPlugin.cpp)

mfd_add_editor_test_plugin(
    mfd_editor_test_bad_abi_plugin
    "mfd_editor_test_bad_abi_plugin"
    ${CMAKE_CURRENT_SOURCE_DIR}/plugins/BadAbiAutomationPlugin.cpp)

mfd_add_editor_test_plugin(
    mfd_editor_test_start_failure_plugin
    "mfd_editor_test_start_failure_plugin"
    ${CMAKE_CURRENT_SOURCE_DIR}/plugins/StartFailureAutomationPlugin.cpp)

mfd_add_editor_test_plugin(
    mfd_editor_test_incomplete_plugin
    "mfd_editor_test_incomplete_plugin"
    ${CMAKE_CURRENT_SOURCE_DIR}/plugins/IncompleteAutomationPlugin.cpp)

mfd_add_editor_test_plugin(
    mfd_editor_test_rollback_on_unload_plugin
    "mfd_editor_test_rollback_on_unload_plugin"
    ${CMAKE_CURRENT_SOURCE_DIR}/plugins/RollbackOnUnloadAutomationPlugin.cpp)

mfd_add_editor_test_plugin(
    mfd_editor_test_stateless_plugin
    "mfd_editor_test_stateless_plugin"
    ${CMAKE_CURRENT_SOURCE_DIR}/plugins/StatelessAutomationPlugin.cpp)

mfd_add_editor_test_plugin(
    mfd_editor_test_save_all_plugin
    "mfd_editor_test_save_all_plugin"
    ${CMAKE_CURRENT_SOURCE_DIR}/plugins/SaveAllAutomationPlugin.cpp)

target_link_libraries(mfd_editor_tests
    PRIVATE
        mfd::editor_plugin_api)

mfd_begin_external_libraries(mfd_editor_tests)
mfd_private_library(mfd_editor_tests GTest::gtest_main)
mfd_private_library(mfd_editor_tests nlohmann_json::nlohmann_json)
mfd_private_library(mfd_editor_tests imgui)
mfd_end_external_libraries(mfd_editor_tests)

target_include_directories(mfd_editor_tests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${MFD_ROOT_DIR}/mfd_editor/include
        ${MFD_ROOT_DIR}/mfd_api/include)

target_sources(mfd_editor_tests
    PRIVATE
        ${MFD_ROOT_DIR}/mfd_editor/include/EditorAutomationBridge.h
        ${MFD_ROOT_DIR}/mfd_editor/include/EditorAutomationDocumentUtils.h
        ${MFD_ROOT_DIR}/mfd_editor/include/EditorAutomationPluginLoader.h
        ${MFD_ROOT_DIR}/mfd_editor/include/EditorAutomationServices.h
        ${MFD_ROOT_DIR}/mfd_editor/include/EditorAutomationTypes.h
        ${MFD_ROOT_DIR}/mfd_editor/src/EditorAutomationCore.cpp
        ${MFD_ROOT_DIR}/mfd_editor/src/EditorAutomationDocumentUtils.cpp
        ${MFD_ROOT_DIR}/mfd_editor/src/EditorTutorialData.cpp
        ${MFD_ROOT_DIR}/mfd_editor/src/EditorAutomationPluginLoader.cpp
        ${MFD_ROOT_DIR}/mfd_editor/src/EditorDocumentSerializer.cpp
        ${MFD_ROOT_DIR}/mfd_editor/src/EditorUiTheme.cpp
        ${MFD_ROOT_DIR}/mfd_api/src/model/PageName.cpp
        ${MFD_ROOT_DIR}/mfd_api/src/model/Reticle.cpp
        ${MFD_ROOT_DIR}/mfd_api/src/model/Types.cpp)

target_compile_features(mfd_editor_tests PRIVATE cxx_std_17)

if(MFD_ENABLE_WARNINGS)
    if(MSVC)
        target_compile_options(mfd_editor_tests
            PRIVATE
                /W4
                /permissive-
                /Zc:__cplusplus
                /EHsc
                /wd4251
                /wd4275)
    else()
        target_compile_options(mfd_editor_tests PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endif()

set_target_properties(mfd_editor_tests PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:mfd_editor_tests>")

add_dependencies(mfd_editor_tests ${MFD_EDITOR_TEST_AUTOMATION_PLUGIN_TARGETS})

if(WIN32)
    set(mfd_editor_test_plugin_files)
    foreach(plugin_target IN LISTS MFD_EDITOR_TEST_AUTOMATION_PLUGIN_TARGETS)
        list(APPEND mfd_editor_test_plugin_files $<TARGET_FILE:${plugin_target}>)
    endforeach()

    add_custom_command(
        TARGET mfd_editor_tests
        POST_BUILD
        COMMAND
            ${CMAKE_COMMAND}
            "-DDEST_DIR=$<TARGET_FILE_DIR:mfd_editor_tests>"
            "-DRUNTIME_DLLS=$<JOIN:$<TARGET_RUNTIME_DLLS:mfd_editor_tests>,$<SEMICOLON>>"
            -P
            "${MFD_ROOT_DIR}/cmake/CopyRuntimeDependencies.cmake"
        COMMAND
            ${CMAKE_COMMAND} -E copy_if_different
            ${mfd_editor_test_plugin_files}
            $<TARGET_FILE_DIR:mfd_editor_tests>
        VERBATIM)
endif()

mfd_stage_runtime(mfd_editor_tests WITH_RUNTIME_DLLS STAGE_SUBDIR tests)

gtest_discover_tests(mfd_editor_tests
    WORKING_DIRECTORY "$<TARGET_FILE_DIR:mfd_editor_tests>"
    DISCOVERY_TIMEOUT 60)

add_custom_target(mfd_editor_tests_run
    COMMAND $<TARGET_FILE:mfd_editor_tests>
    DEPENDS mfd_editor_tests
    USES_TERMINAL)
