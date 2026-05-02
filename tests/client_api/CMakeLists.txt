# This file is part of MFDStudio.
# Project author: Benoit Fra
# Repository: https://github.com/benoitfragit/MFDStudio
include(GoogleTest)

set(CLIENT_API_GENERATED_UI_FIXTURE_DIR
    ${CMAKE_CURRENT_SOURCE_DIR}/fixtures/generated_ui)

set(CLIENT_API_GENERATED_UI_FIXTURE_OUTPUT_DIR
    ${CMAKE_CURRENT_BINARY_DIR}/generated_fixture)

set(CLIENT_API_GENERATED_UI_FIXTURE_HEADER
    ${CLIENT_API_GENERATED_UI_FIXTURE_OUTPUT_DIR}/GeneratedUiFixture.h)

set(CLIENT_API_GENERATED_UI_FIXTURE_SOURCE
    ${CLIENT_API_GENERATED_UI_FIXTURE_OUTPUT_DIR}/GeneratedUiFixture.cpp)

set(CLIENT_API_GENERATED_UI_FIXTURE_MAP
    ${CLIENT_API_GENERATED_UI_FIXTURE_OUTPUT_DIR}/GeneratedUiFixture.generated.map)

client_api_generate_ui(
    WINDOW_JSON "${CLIENT_API_GENERATED_UI_FIXTURE_DIR}/window.json"
    OUTPUT_HEADER "${CLIENT_API_GENERATED_UI_FIXTURE_HEADER}"
    OUTPUT_SOURCE "${CLIENT_API_GENERATED_UI_FIXTURE_SOURCE}"
    OUTPUT_MAP "${CLIENT_API_GENERATED_UI_FIXTURE_MAP}"
    NAMESPACE "generated_ui_fixture"
    UI_CLASS_NAME "GeneratedUiFixture"
    HEADER_INCLUDE "GeneratedUiFixture.h")

set(CLIENT_API_TEST_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/AnimationTests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/GeneratedUiCompiledApiTests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/GeneratedUiRuntimeTests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/LatestBatchPublisherTests.cpp
    ${CLIENT_API_GENERATED_UI_FIXTURE_SOURCE})

add_executable(client_api_tests ${CLIENT_API_TEST_SOURCES})

mfd_configure_test_runtime_output(client_api_tests)

target_include_directories(client_api_tests
    PRIVATE
        ${CLIENT_API_GENERATED_UI_FIXTURE_OUTPUT_DIR})

target_link_libraries(client_api_tests
    PRIVATE
        mfd::client)

mfd_private_library(client_api_tests GTest::gtest_main)

target_compile_features(client_api_tests PRIVATE cxx_std_17)

mfd_enable_target_warnings(client_api_tests)

set_target_properties(client_api_tests PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:client_api_tests>")

if(WIN32)
    add_custom_command(
        TARGET client_api_tests
        POST_BUILD
        COMMAND
            ${CMAKE_COMMAND}
            "-DDEST_DIR=$<TARGET_FILE_DIR:client_api_tests>"
            "-DRUNTIME_DLLS=$<JOIN:$<TARGET_RUNTIME_DLLS:client_api_tests>,$<SEMICOLON>>"
            -P
            "${MFD_ROOT_DIR}/cmake/CopyRuntimeDependencies.cmake"
        VERBATIM)
endif()

mfd_stage_runtime(client_api_tests WITH_RUNTIME_DLLS STAGE_SUBDIR tests)

gtest_discover_tests(client_api_tests
    WORKING_DIRECTORY "$<TARGET_FILE_DIR:client_api_tests>"
    DISCOVERY_TIMEOUT 60)

add_custom_target(client_api_tests_run
    COMMAND $<TARGET_FILE:client_api_tests>
    DEPENDS client_api_tests
    USES_TERMINAL)
