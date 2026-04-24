# This file is part of MFDStudio.
# Project author: Benoit Fra
# Repository: https://github.com/benoitfragit/MFDStudio
include(GoogleTest)

set(CLIENT_API_TEST_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/AnimationTests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/GeneratedUiRuntimeTests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/LatestBatchPublisherTests.cpp)

add_executable(client_api_tests ${CLIENT_API_TEST_SOURCES})

target_link_libraries(client_api_tests
    PRIVATE
        mfd::client
        GTest::gtest_main)

target_compile_features(client_api_tests PRIVATE cxx_std_17)

if(MFD_ENABLE_WARNINGS)
    if(MSVC)
        target_compile_options(client_api_tests
            PRIVATE
                /W4
                /permissive-
                /Zc:__cplusplus
                /EHsc
                /wd4251
                /wd4275)
    else()
        target_compile_options(client_api_tests PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endif()

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

mfd_stage_runtime(client_api_tests WITH_RUNTIME_DLLS)

gtest_discover_tests(client_api_tests
    WORKING_DIRECTORY "$<TARGET_FILE_DIR:client_api_tests>"
    DISCOVERY_TIMEOUT 60)

add_custom_target(client_api_tests_run
    COMMAND $<TARGET_FILE:client_api_tests>
    DEPENDS client_api_tests
    USES_TERMINAL)
