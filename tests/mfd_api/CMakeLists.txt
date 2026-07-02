# This file is part of MFDStudio.
# Project author: Benoit Fra
# Repository: https://github.com/benoitfragit/MFDStudio
include(GoogleTest)

set(MFD_API_TEST_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/ArrayViewTests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/BezierPolylineCacheTests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/CanvasCircleStrokeRenderTests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/CanvasClippingCallbackRenderTests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/CanvasClippingRenderTests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/CanvasRingRenderTests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/CanvasTextRenderTests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/CommandClientTests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/CommandProcessorTests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/CommandTransportTests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/CommandTypesTests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/DocumentSemanticValidatorTests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/GeneratedTransportMapTests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/JsonNumberParsingTests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/JsonLoaderTests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/PolygonTriangulationTests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/PublicRenderFacadeHeadersTests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/RenderResourceLifetimeTests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ReticleTests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/RuntimeValidationTests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/SceneRegistryTests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/StrobeFeedbackTests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/TextAnchorSnapTests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/TextLayoutCacheTests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/UdpRuntimeBridgeTests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/UserSpaceProjectorTests.cpp)

add_executable(mfd_api_tests ${MFD_API_TEST_SOURCES})

mfd_configure_test_runtime_output(mfd_api_tests)

target_link_libraries(mfd_api_tests
    PRIVATE
        mfd::model
        mfd::transport
        mfd::runtime
        mfd::io_json
        mfd::render_raylib
        protobuf::libprotobuf)

mfd_private_library(mfd_api_tests GTest::gtest_main)
mfd_private_library(mfd_api_tests nlohmann_json::nlohmann_json)

target_compile_features(mfd_api_tests PRIVATE cxx_std_17)

target_include_directories(mfd_api_tests
    PRIVATE
        ${CMAKE_BINARY_DIR}/mfd_common_api/generated
        ${MFD_ROOT_DIR}/mfd_api/src/control
        ${MFD_ROOT_DIR}/mfd_api/src/io/json
        ${MFD_ROOT_DIR}/mfd_api/src/runtime
        ${MFD_ROOT_DIR}/mfd_api/src/render)

mfd_enable_target_warnings(mfd_api_tests)

set_target_properties(mfd_api_tests PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:mfd_api_tests>")

if(WIN32)
    add_custom_command(
        TARGET mfd_api_tests
        POST_BUILD
        COMMAND
            ${CMAKE_COMMAND}
            "-DDEST_DIR=$<TARGET_FILE_DIR:mfd_api_tests>"
            "-DRUNTIME_DLLS=$<JOIN:$<TARGET_RUNTIME_DLLS:mfd_api_tests>,$<SEMICOLON>>"
            -P
            "${MFD_ROOT_DIR}/cmake/CopyRuntimeDependencies.cmake"
        VERBATIM)
endif()

gtest_discover_tests(mfd_api_tests
    WORKING_DIRECTORY "$<TARGET_FILE_DIR:mfd_api_tests>"
    DISCOVERY_TIMEOUT 60)

add_custom_target(mfd_api_tests_run
    COMMAND $<TARGET_FILE:mfd_api_tests>
    DEPENDS mfd_api_tests
    USES_TERMINAL)
