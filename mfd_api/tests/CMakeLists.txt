# This file is part of MFDStudio.
# Project author: Benoit Fra
# Repository: https://github.com/benoitfragit/MFDStudio
include(GoogleTest)

set(MFD_API_TEST_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/ArrayViewTests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/CommandClientTests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/CommandProcessorTests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/CommandTypesTests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/JsonLoaderTests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ReticleTests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/SceneRegistryTests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/UdpRuntimeBridgeTests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/UserSpaceProjectorTests.cpp)

add_executable(mfd_api_tests ${MFD_API_TEST_SOURCES})

target_link_libraries(mfd_api_tests
    PRIVATE
        mfd::api
        GTest::gtest_main)

target_compile_features(mfd_api_tests PRIVATE cxx_std_17)

if(MFD_ENABLE_WARNINGS)
    if(MSVC)
        target_compile_options(mfd_api_tests
            PRIVATE
                /W4
                /permissive-
                /Zc:__cplusplus
                /EHsc
                /wd4251
                /wd4275)
    else()
        target_compile_options(mfd_api_tests PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endif()

set_target_properties(mfd_api_tests PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:mfd_api_tests>")

if(WIN32)
    add_custom_command(
        TARGET mfd_api_tests
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_RUNTIME_DLLS:mfd_api_tests>
            "$<TARGET_FILE_DIR:mfd_api_tests>"
        COMMAND_EXPAND_LISTS)
endif()

mfd_stage_runtime(mfd_api_tests WITH_RUNTIME_DLLS)

gtest_discover_tests(mfd_api_tests
    WORKING_DIRECTORY "$<TARGET_FILE_DIR:mfd_api_tests>"
    DISCOVERY_TIMEOUT 60)

add_custom_target(mfd_api_tests_run
    COMMAND $<TARGET_FILE:mfd_api_tests>
    DEPENDS mfd_api_tests
    USES_TERMINAL)
