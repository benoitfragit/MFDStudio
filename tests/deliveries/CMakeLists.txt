# This file is part of MFDStudio.
# Project author: Benoit Fra
# Repository: https://github.com/benoitfragit/MFDStudio
if(NOT WIN32)
    return()
endif()

set(MFD_DELIVERY_TEST_COMMON_ARGS
    "-DBUILD_DIRECTORY=${CMAKE_BINARY_DIR}"
    "-DBUILD_CONFIG=$<CONFIG>"
    "-DMFD_SOURCE_DIR=${MFD_ROOT_DIR}"
    "-DPARENT_CMAKE_GENERATOR=${CMAKE_GENERATOR}"
    "-DPARENT_CMAKE_GENERATOR_PLATFORM=${CMAKE_GENERATOR_PLATFORM}"
    "-DPARENT_CMAKE_GENERATOR_TOOLSET=${CMAKE_GENERATOR_TOOLSET}")

add_test(
    NAME exec_stage_smoke
    COMMAND
        ${CMAKE_COMMAND}
        "-DBUILD_DIRECTORY=${CMAKE_BINARY_DIR}"
        "-DBUILD_CONFIG=$<CONFIG>"
        "-DEXEC_ROOT=${MFD_ROOT_DIR}/_Exec"
        "-DTOOLSET=${MFD_EXEC_COMPILER_DIR}"
        "-DPLATFORM=${MFD_EXEC_PLATFORM_DIR}"
        -P
        "${MFD_ROOT_DIR}/cmake/VerifyExecLayout.cmake")
set_tests_properties(exec_stage_smoke PROPERTIES LABELS "runtime;stage;smoke")

add_test(
    NAME delivery_layout_smoke
    COMMAND
        ${CMAKE_COMMAND}
        "-DBUILD_DIRECTORY=${CMAKE_BINARY_DIR}"
        "-DBUILD_CONFIG=$<CONFIG>"
        "-DDELIVERIES_ROOT=${MFD_ROOT_DIR}/_Deliveries"
        "-DTOOLSET=${MFD_EXEC_COMPILER_DIR}"
        "-DPLATFORM=${MFD_EXEC_PLATFORM_DIR}"
        -P
        "${MFD_ROOT_DIR}/cmake/VerifyDeliveryLayout.cmake")
set_tests_properties(delivery_layout_smoke PROPERTIES LABELS "delivery;smoke")

add_test(
    NAME delivery_client_api_sdk_smoke
    COMMAND
        ${CMAKE_COMMAND}
        ${MFD_DELIVERY_TEST_COMMON_ARGS}
        "-DFIXTURE_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}/client_api_sdk"
        "-DFIXTURE_BUILD_DIR=${CMAKE_CURRENT_BINARY_DIR}/client_api_sdk_build"
        "-DMFD_PACKAGE_CMAKE_DIR=${MFD_ROOT_DIR}/_Deliveries/packages_windows/MFDStudioClientApi/build/native/cmake"
        -P
        "${MFD_ROOT_DIR}/cmake/RunPackageConsumerBuild.cmake")
set_tests_properties(delivery_client_api_sdk_smoke PROPERTIES LABELS "delivery;sdk;smoke")

add_test(
    NAME delivery_editor_plugin_sdk_smoke
    COMMAND
        ${CMAKE_COMMAND}
        ${MFD_DELIVERY_TEST_COMMON_ARGS}
        "-DFIXTURE_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}/editor_plugin_sdk"
        "-DFIXTURE_BUILD_DIR=${CMAKE_CURRENT_BINARY_DIR}/editor_plugin_sdk_build"
        "-DMFD_PACKAGE_CMAKE_DIR=${MFD_ROOT_DIR}/_Deliveries/packages_windows/MFDStudioEditorPlugin/build/native/cmake"
        -P
        "${MFD_ROOT_DIR}/cmake/RunPackageConsumerBuild.cmake")
set_tests_properties(delivery_editor_plugin_sdk_smoke PROPERTIES LABELS "delivery;sdk;smoke")

add_test(
    NAME delivery_window_launcher_plugin_sdk_smoke
    COMMAND
        ${CMAKE_COMMAND}
        ${MFD_DELIVERY_TEST_COMMON_ARGS}
        "-DFIXTURE_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}/window_launcher_plugin_sdk"
        "-DFIXTURE_BUILD_DIR=${CMAKE_CURRENT_BINARY_DIR}/window_launcher_plugin_sdk_build"
        "-DMFD_PACKAGE_CMAKE_DIR=${MFD_ROOT_DIR}/_Deliveries/packages_windows/MFDStudioWindowLauncherPlugin/build/native/cmake"
        -P
        "${MFD_ROOT_DIR}/cmake/RunPackageConsumerBuild.cmake")
set_tests_properties(delivery_window_launcher_plugin_sdk_smoke PROPERTIES LABELS "delivery;sdk;smoke")
