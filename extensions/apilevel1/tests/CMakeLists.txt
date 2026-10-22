# This file is part of MFDStudio.
# Project author: Benoit Fra
# Repository: https://github.com/benoitfragit/MFDStudio

include(GoogleTest)

add_executable(mfd_apilevel1_tests
    ${CMAKE_CURRENT_SOURCE_DIR}/ContainerCommandTests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ContainerInputRouterTests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ContainerEditorExtensionTests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ContainerGizmoAdapterTests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ContainerEditorModelTests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ContainerLayoutTests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ContainerRuntimeAbiTests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ContainerRuntimeSessionTests.cpp
    ${MFD_APILEVEL1_ROOT_DIR}/editor/src/ContainerEditorModel.cpp
    ${MFD_WIDGETS_ROOT_DIR}/editor/src/WidgetEditorModel.cpp
    ${MFD_APILEVEL1_ROOT_DIR}/editor/src/ContainerGizmoAdapter.cpp
    ${MFD_APILEVEL1_ROOT_DIR}/runtime/src/ContainerInputRouter.cpp)

mfd_configure_test_runtime_output(mfd_apilevel1_tests)
target_compile_features(mfd_apilevel1_tests PRIVATE cxx_std_17)
target_include_directories(mfd_apilevel1_tests PRIVATE
    ${MFD_APILEVEL1_RUNTIME_INCLUDE_DIR}
    ${MFD_APILEVEL1_EDITOR_INCLUDE_DIR}
    ${MFD_WIDGETS_ROOT_DIR}/editor/include)
target_link_libraries(mfd_apilevel1_tests PRIVATE
    mfd::api
    mfd::io_json
    mfd::model
    mfd::runtime_api
    mfd::transport
    mfd_containers_api1
    mfd_editor_containers_api1)
mfd_private_library(mfd_apilevel1_tests GTest::gtest_main)
mfd_private_library(mfd_apilevel1_tests nlohmann_json::nlohmann_json)
mfd_enable_target_warnings(mfd_apilevel1_tests)

if(WIN32)
    add_custom_command(
        TARGET mfd_apilevel1_tests
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_FILE:mfd_api>
                $<TARGET_FILE:mfd_runtime_api>
                $<TARGET_FILE:mfd_containers_api1>
                $<TARGET_FILE:mfd_editor_containers_api1>
                $<TARGET_FILE_DIR:mfd_apilevel1_tests>/
        VERBATIM)
endif()

gtest_discover_tests(mfd_apilevel1_tests
    WORKING_DIRECTORY "$<TARGET_FILE_DIR:mfd_apilevel1_tests>"
    DISCOVERY_TIMEOUT 60)
add_executable(mfd_apilevel1_extension_availability_tests
    ${CMAKE_CURRENT_SOURCE_DIR}/ExtensionAvailabilityTests.cpp)
target_compile_features(mfd_apilevel1_extension_availability_tests PRIVATE cxx_std_17)
target_link_libraries(mfd_apilevel1_extension_availability_tests PRIVATE
    mfd::runtime_api
    GTest::gtest_main)
mfd_enable_target_warnings(mfd_apilevel1_extension_availability_tests)
set_target_properties(mfd_apilevel1_extension_availability_tests PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/without_extension")

if(WIN32)
    add_custom_command(
        TARGET mfd_apilevel1_extension_availability_tests
        POST_BUILD
        COMMAND ${CMAKE_COMMAND}
                "-DDEST_DIR=$<TARGET_FILE_DIR:mfd_apilevel1_extension_availability_tests>"
                "-DRUNTIME_DLLS=$<JOIN:$<TARGET_RUNTIME_DLLS:mfd_apilevel1_extension_availability_tests>,$<SEMICOLON>>"
                -P "${MFD_ROOT_DIR}/cmake/CopyRuntimeDependencies.cmake"
        VERBATIM)
endif()

gtest_discover_tests(mfd_apilevel1_extension_availability_tests
    WORKING_DIRECTORY "$<TARGET_FILE_DIR:mfd_apilevel1_extension_availability_tests>"
    DISCOVERY_TIMEOUT 60)

if(WIN32)
    find_package(Python3 COMPONENTS Interpreter REQUIRED)
    add_test(NAME apilevel1_installed_extensions
        COMMAND ${CMAKE_COMMAND}
            "-DBUILD_DIRECTORY=${CMAKE_BINARY_DIR}"
            "-DBUILD_CONFIG=$<CONFIG>"
            "-DSOURCE_DIRECTORY=${MFD_ROOT_DIR}"
            "-DTEST_ROOT=${CMAKE_CURRENT_BINARY_DIR}/installed_extensions/$<CONFIG>"
            "-DPYTHON_EXECUTABLE=${Python3_EXECUTABLE}"
            "-DGENERATOR=${CMAKE_GENERATOR}"
            "-DGENERATOR_PLATFORM=${CMAKE_GENERATOR_PLATFORM}"
            "-DGENERATOR_TOOLSET=${CMAKE_GENERATOR_TOOLSET}"
            "-DMSVC_RUNTIME=${CMAKE_MSVC_RUNTIME_LIBRARY}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/VerifyInstalledExtensions.cmake")
    set_tests_properties(apilevel1_installed_extensions PROPERTIES LABELS "smoke;runtime" TIMEOUT 180)
    add_test(NAME apilevel1_host_deployment
        COMMAND ${CMAKE_COMMAND}
            "-DRUNTIME_DLL=$<TARGET_FILE:mfd_containers_api1>"
            "-DEDITOR_DLL=$<TARGET_FILE:mfd_editor_containers_api1>"
            "-DWIDGET_RUNTIME_DLL=$<TARGET_FILE:mfd_widgets_api1>"
            "-DWIDGET_EDITOR_DLL=$<TARGET_FILE:mfd_editor_widgets_api1>"
            "-DWINDOW_DIR=$<TARGET_FILE_DIR:mfd_window>"
            "-DEDITOR_DIR=$<TARGET_FILE_DIR:mfd_editor>"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/VerifyHostDeployment.cmake")
endif()

# Headless command-sink tests exercise the actual host decoration implementation.
add_executable(mfd_container_decoration_tests
    ${CMAKE_CURRENT_SOURCE_DIR}/ContainerDecorationTests.cpp
    ${MFD_APILEVEL1_ROOT_DIR}/runtime/src/ContainerDecorationRenderer.cpp)
target_compile_features(mfd_container_decoration_tests PRIVATE cxx_std_17)
target_include_directories(mfd_container_decoration_tests PRIVATE
    ${MFD_APILEVEL1_RUNTIME_INCLUDE_DIR}
    ${MFD_APILEVEL1_ROOT_DIR}/runtime/src
    $<TARGET_PROPERTY:raylib,INTERFACE_INCLUDE_DIRECTORIES>)
target_link_libraries(mfd_container_decoration_tests PRIVATE mfd::model GTest::gtest_main)
mfd_enable_target_warnings(mfd_container_decoration_tests)
gtest_discover_tests(mfd_container_decoration_tests)

add_executable(mfd_container_inspector_gesture_tests
    ${CMAKE_CURRENT_SOURCE_DIR}/ContainerInspectorGestureTests.cpp
    ${MFD_APILEVEL1_ROOT_DIR}/editor/src/ContainerInspectorGesture.cpp)
target_compile_features(mfd_container_inspector_gesture_tests PRIVATE cxx_std_17)
target_include_directories(mfd_container_inspector_gesture_tests PRIVATE ${MFD_APILEVEL1_ROOT_DIR}/editor/src)
target_link_libraries(mfd_container_inspector_gesture_tests PRIVATE GTest::gtest_main)
mfd_enable_target_warnings(mfd_container_inspector_gesture_tests)
gtest_discover_tests(mfd_container_inspector_gesture_tests)
