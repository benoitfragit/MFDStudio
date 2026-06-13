# This file is part of MFDStudio.
# Project author: Benoit Fra
# Repository: https://github.com/benoitfragit/MFDStudio
include(GoogleTest)

set(MFD_RUNTIME_API_TEST_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/OffscreenSurfaceTests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/PublicHeadersTests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/RuntimeSessionTests.cpp)

add_executable(mfd_runtime_api_tests ${MFD_RUNTIME_API_TEST_SOURCES})

mfd_configure_test_runtime_output(mfd_runtime_api_tests)

target_link_libraries(mfd_runtime_api_tests
    PRIVATE
        EnTT::EnTT
        mfd::runtime_api
        GTest::gtest_main
        protobuf::libprotobuf
        raylib)

target_compile_features(mfd_runtime_api_tests PRIVATE cxx_std_17)

target_include_directories(mfd_runtime_api_tests
    PRIVATE
        ${MFD_COMMON_API_PUBLIC_INCLUDE_DIR}
        ${MFD_API_PUBLIC_INCLUDE_DIR}
        ${MFD_ROOT_DIR}/mfd_runtime_api/src)

mfd_enable_target_warnings(mfd_runtime_api_tests)

set_target_properties(mfd_runtime_api_tests PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:mfd_runtime_api_tests>")

if(WIN32)
    add_custom_command(
        TARGET mfd_runtime_api_tests
        POST_BUILD
        COMMAND
            ${CMAKE_COMMAND}
            "-DDEST_DIR=$<TARGET_FILE_DIR:mfd_runtime_api_tests>"
            "-DRUNTIME_DLLS=$<JOIN:$<TARGET_RUNTIME_DLLS:mfd_runtime_api_tests>,$<SEMICOLON>>"
            -P
            "${MFD_ROOT_DIR}/cmake/CopyRuntimeDependencies.cmake"
        VERBATIM)
endif()

gtest_discover_tests(mfd_runtime_api_tests
    WORKING_DIRECTORY "$<TARGET_FILE_DIR:mfd_runtime_api_tests>"
    DISCOVERY_TIMEOUT 60)

add_custom_target(mfd_runtime_api_tests_run
    COMMAND $<TARGET_FILE:mfd_runtime_api_tests>
    DEPENDS mfd_runtime_api_tests
    USES_TERMINAL)
