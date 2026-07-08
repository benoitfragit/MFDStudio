# This file is part of MFDStudio.
# Project author: Benoit Fra
# Repository: https://github.com/benoitfragit/MFDStudio
include(GoogleTest)

# White-box tests: they compile the hud_runtime sources and the sample-client
# simulation directly so they can exercise generated-UI internals that the
# shared library intentionally does not export. HUD_RUNTIME_BUILDING_DLL keeps
# the HUD_RUNTIME_API decoration in export mode for these local definitions.
add_executable(hud_client_tests
    ${CMAKE_CURRENT_SOURCE_DIR}/HudSimulationTests.cpp
    ${MFD_ROOT_DIR}/examples/hud/runtime/generated/HudUi.cpp
    ${MFD_ROOT_DIR}/examples/hud/runtime/src/HudController.cpp
    ${MFD_ROOT_DIR}/examples/hud/runtime/src/HudProjection.cpp
    ${MFD_ROOT_DIR}/examples/hud/main/src/HudSimulation.cpp)

mfd_configure_test_runtime_output(hud_client_tests)

target_compile_definitions(hud_client_tests
    PRIVATE
        HUD_RUNTIME_BUILDING_DLL)

target_include_directories(hud_client_tests
    PRIVATE
        ${MFD_ROOT_DIR}/examples/hud/runtime/generated
        ${MFD_ROOT_DIR}/examples/hud/runtime/include
        ${MFD_ROOT_DIR}/examples/hud/main/include)

mfd_private_library(hud_client_tests GTest::gtest_main mfd::client)

target_compile_features(hud_client_tests PRIVATE cxx_std_17)

mfd_enable_target_warnings(hud_client_tests)

set_target_properties(hud_client_tests PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:hud_client_tests>")

if(WIN32)
    add_custom_command(
        TARGET hud_client_tests
        POST_BUILD
        COMMAND
            ${CMAKE_COMMAND}
            "-DDEST_DIR=$<TARGET_FILE_DIR:hud_client_tests>"
            "-DRUNTIME_DLLS=$<JOIN:$<TARGET_RUNTIME_DLLS:hud_client_tests>,$<SEMICOLON>>"
            -P
            "${MFD_ROOT_DIR}/cmake/CopyRuntimeDependencies.cmake"
        VERBATIM)
endif()

mfd_stage_runtime(hud_client_tests WITH_RUNTIME_DLLS STAGE_SUBDIR tests)

gtest_discover_tests(hud_client_tests
    WORKING_DIRECTORY "$<TARGET_FILE_DIR:hud_client_tests>"
    DISCOVERY_TIMEOUT 60)

add_custom_target(hud_client_tests_run
    COMMAND $<TARGET_FILE:hud_client_tests>
    DEPENDS hud_client_tests
    USES_TERMINAL)

# Link-level tests: they consume hud_runtime only through its exported shared
# library surface, proving that hud_runtime builds as a DLL/shared library,
# that the public symbols are exported/imported correctly and that the funnel
# geometry is computed from a generic HudInputSample without any main-client
# type, ImGui or DX11 dependency.
add_executable(hud_runtime_client_tests
    ${CMAKE_CURRENT_SOURCE_DIR}/HudRuntimeClientTests.cpp)

mfd_configure_test_runtime_output(hud_runtime_client_tests)

mfd_private_library(hud_runtime_client_tests GTest::gtest_main hud_runtime)

target_compile_features(hud_runtime_client_tests PRIVATE cxx_std_17)

mfd_enable_target_warnings(hud_runtime_client_tests)

set_target_properties(hud_runtime_client_tests PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:hud_runtime_client_tests>")

if(WIN32)
    add_custom_command(
        TARGET hud_runtime_client_tests
        POST_BUILD
        COMMAND
            ${CMAKE_COMMAND}
            "-DDEST_DIR=$<TARGET_FILE_DIR:hud_runtime_client_tests>"
            "-DRUNTIME_DLLS=$<JOIN:$<TARGET_RUNTIME_DLLS:hud_runtime_client_tests>,$<SEMICOLON>>"
            -P
            "${MFD_ROOT_DIR}/cmake/CopyRuntimeDependencies.cmake"
        VERBATIM)
endif()

mfd_stage_runtime(hud_runtime_client_tests WITH_RUNTIME_DLLS STAGE_SUBDIR tests)

gtest_discover_tests(hud_runtime_client_tests
    WORKING_DIRECTORY "$<TARGET_FILE_DIR:hud_runtime_client_tests>"
    DISCOVERY_TIMEOUT 60)
