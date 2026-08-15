# This file is part of MFDStudio.
# Project author: Benoit Fra
# Repository: https://github.com/benoitfragit/MFDStudio
include(GoogleTest)

# The projection and mini-simulation are pure C++17 with no generated UI, Win32
# or Direct3D dependency, so this regression target builds and runs on every
# platform even though the interactive client is Win32-only.
add_executable(lhld_client_tests
    ${CMAKE_CURRENT_SOURCE_DIR}/LhldProjectionTests.cpp
    ${MFD_ROOT_DIR}/examples/lhld/client/src/MfdProjection.cpp
    ${MFD_ROOT_DIR}/examples/lhld/client/src/MfdRadarControls.cpp
    ${MFD_ROOT_DIR}/examples/lhld/client/src/MfdRadarGeometry.cpp
    ${MFD_ROOT_DIR}/examples/lhld/client/src/MfdRadarSimulation.cpp)

mfd_configure_test_runtime_output(lhld_client_tests)

target_include_directories(lhld_client_tests
    PRIVATE
        ${MFD_ROOT_DIR}/examples/lhld/client/src)

mfd_private_library(lhld_client_tests GTest::gtest_main)

target_compile_features(lhld_client_tests PRIVATE cxx_std_17)

mfd_enable_target_warnings(lhld_client_tests)

set_target_properties(lhld_client_tests PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:lhld_client_tests>")

gtest_discover_tests(lhld_client_tests
    WORKING_DIRECTORY "$<TARGET_FILE_DIR:lhld_client_tests>"
    DISCOVERY_TIMEOUT 60)

add_custom_target(lhld_client_tests_run
    COMMAND $<TARGET_FILE:lhld_client_tests>
    DEPENDS lhld_client_tests
    USES_TERMINAL)
