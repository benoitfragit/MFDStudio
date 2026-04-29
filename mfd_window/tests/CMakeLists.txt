# This file is part of MFDStudio.
# Project author: Benoit Fra
# Repository: https://github.com/benoitfragit/MFDStudio
include(GoogleTest)

set(MFD_WINDOW_TEST_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/RuntimeDebugTests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/WindowLauncherTests.cpp)

add_executable(mfd_window_tests ${MFD_WINDOW_TEST_SOURCES})

mfd_configure_test_runtime_output(mfd_window_tests)

target_include_directories(mfd_window_tests
    PRIVATE
        ${MFD_ROOT_DIR}/mfd_window/src)

target_link_libraries(mfd_window_tests
    PRIVATE
        mfd::window_launcher)

mfd_begin_external_libraries(mfd_window_tests)
mfd_private_library(mfd_window_tests GTest::gtest_main)
mfd_end_external_libraries(mfd_window_tests)

target_compile_features(mfd_window_tests PRIVATE cxx_std_17)

if(MFD_ENABLE_WARNINGS)
    if(MSVC)
        target_compile_options(mfd_window_tests
            PRIVATE
                /W4
                /permissive-
                /Zc:__cplusplus
                /EHsc
                /wd4251
                /wd4275)
    else()
        target_compile_options(mfd_window_tests PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endif()

set_target_properties(mfd_window_tests PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:mfd_window_tests>")

if(WIN32)
    add_custom_command(
        TARGET mfd_window_tests
        POST_BUILD
        COMMAND
            ${CMAKE_COMMAND}
            "-DDEST_DIR=$<TARGET_FILE_DIR:mfd_window_tests>"
            "-DRUNTIME_DLLS=$<JOIN:$<TARGET_RUNTIME_DLLS:mfd_window_tests>,$<SEMICOLON>>"
            -P
            "${MFD_ROOT_DIR}/cmake/CopyRuntimeDependencies.cmake"
        VERBATIM)
endif()

mfd_stage_runtime(mfd_window_tests WITH_RUNTIME_DLLS STAGE_SUBDIR tests)

mfd_add_runtime_layout_smoke_test(
    mfd_window
    EXTRA_RUNTIME_TARGETS mfd_api
    EXTRA_FILES
        Start-MfdWindow.bat
        Start-MfdDemo.bat
        Start-MfdCockpit.bat
        Start-MfdMinimal.bat
        Start-MfdTutorial.bat
    VERIFY_ASSETS
    LABELS runtime)

gtest_discover_tests(mfd_window_tests
    WORKING_DIRECTORY "$<TARGET_FILE_DIR:mfd_window_tests>"
    DISCOVERY_TIMEOUT 60)

add_custom_target(mfd_window_tests_run
    COMMAND $<TARGET_FILE:mfd_window_tests>
    DEPENDS mfd_window_tests
    USES_TERMINAL)
