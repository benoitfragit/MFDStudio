# This file is part of MFDStudio.
# Project author: Benoit Fra
# Repository: https://github.com/benoitfragit/MFDStudio

set(MFD_RUNTIME_PUBLIC_HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/include/mfd/control/CommandProcessor.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/mfd/control/UdpRuntimeBridge.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/mfd/control/UserSpaceProjector.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/mfd/runtime/DocumentSemanticValidator.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/mfd/runtime/SceneRegistry.h)

set(MFD_RUNTIME_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/control/CommandProcessor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/control/UdpRuntimeBridge.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/control/UserSpaceProjector.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/runtime/DocumentSemanticValidator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/runtime/SceneRegistry.cpp)

add_library(mfd_runtime_objects OBJECT
    ${MFD_RUNTIME_PUBLIC_HEADERS}
    ${MFD_RUNTIME_SOURCES})

mfd_configure_component(
    mfd_runtime_objects
    PUBLIC_INCLUDE_DIRS
        "${MFD_COMMON_API_PUBLIC_INCLUDE_DIR}"
        "${MFD_API_PUBLIC_INCLUDE_DIR}")
if(MFD_ENABLE_FUZZING)
    mfd_enable_fuzzing_instrumentation(mfd_runtime_objects)
endif()
if(MFD_API_BUILD_SHARED)
    target_compile_definitions(mfd_runtime_objects PRIVATE MFD_API_EXPORTS)
endif()
target_link_libraries(mfd_runtime_objects
    PUBLIC
        mfd::model
        mfd::transport
        EnTT::EnTT)

add_library(mfd_runtime STATIC
    ${MFD_RUNTIME_PUBLIC_HEADERS}
    $<TARGET_OBJECTS:mfd_runtime_objects>)
add_library(mfd::runtime ALIAS mfd_runtime)

mfd_configure_component(
    mfd_runtime
    PUBLIC_INCLUDE_DIRS
        "${MFD_COMMON_API_PUBLIC_INCLUDE_DIR}"
        "${MFD_API_PUBLIC_INCLUDE_DIR}")
target_link_libraries(mfd_runtime
    PUBLIC
        mfd::model
        mfd::transport
        EnTT::EnTT)

set(MFD_IO_JSON_PUBLIC_HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/include/mfd/io/JsonLoader.h)

set(MFD_IO_JSON_PRIVATE_HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/io/json/JsonNumberParsing.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/io/json/JsonValueHelpers.h)

set(MFD_IO_JSON_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/io/JsonLoader.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/io/json/JsonNumberParsing.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/io/json/JsonValueHelpers.cpp)

add_library(mfd_io_json_objects OBJECT
    ${MFD_IO_JSON_PUBLIC_HEADERS}
    ${MFD_IO_JSON_PRIVATE_HEADERS}
    ${MFD_IO_JSON_SOURCES})

mfd_configure_component(
    mfd_io_json_objects
    PUBLIC_INCLUDE_DIRS
        "${MFD_COMMON_API_PUBLIC_INCLUDE_DIR}"
        "${MFD_API_PUBLIC_INCLUDE_DIR}")
if(MFD_ENABLE_FUZZING)
    mfd_enable_fuzzing_instrumentation(mfd_io_json_objects)
endif()
if(MFD_API_BUILD_SHARED)
    target_compile_definitions(mfd_io_json_objects PRIVATE MFD_API_EXPORTS)
endif()
target_link_libraries(mfd_io_json_objects
    PUBLIC
        mfd::model
        mfd::transport
    PRIVATE
        nlohmann_json::nlohmann_json)

add_library(mfd_io_json STATIC
    ${MFD_IO_JSON_PUBLIC_HEADERS}
    ${MFD_IO_JSON_PRIVATE_HEADERS}
    $<TARGET_OBJECTS:mfd_io_json_objects>)
add_library(mfd::io_json ALIAS mfd_io_json)

mfd_configure_component(
    mfd_io_json
    PUBLIC_INCLUDE_DIRS
        "${MFD_COMMON_API_PUBLIC_INCLUDE_DIR}"
        "${MFD_API_PUBLIC_INCLUDE_DIR}")
target_link_libraries(mfd_io_json
    PUBLIC
        mfd::model
        mfd::transport
    PRIVATE
        nlohmann_json::nlohmann_json)

set(MFD_API_LIBRARY_TYPE STATIC)
set(MFD_API_DEPENDENCY_VISIBILITY PUBLIC)
if(MFD_API_BUILD_SHARED)
    set(MFD_API_LIBRARY_TYPE SHARED)
    set(MFD_API_DEPENDENCY_VISIBILITY PRIVATE)
endif()

add_library(mfd_api ${MFD_API_LIBRARY_TYPE}
    $<TARGET_OBJECTS:mfd_model_objects>
    $<TARGET_OBJECTS:mfd_transport_objects>
    $<TARGET_OBJECTS:mfd_runtime_objects>
    $<TARGET_OBJECTS:mfd_io_json_objects>)

add_library(mfd::api ALIAS mfd_api)
add_library(mfd::core ALIAS mfd_api)

mfd_configure_component(
    mfd_api
    PUBLIC_INCLUDE_DIRS
        "${MFD_COMMON_API_PUBLIC_INCLUDE_DIR}"
        "${MFD_API_PUBLIC_INCLUDE_DIR}")
target_link_libraries(mfd_api
    PUBLIC
        EnTT::EnTT
    ${MFD_API_DEPENDENCY_VISIBILITY}
        nlohmann_json::nlohmann_json
        protobuf::libprotobuf)

if(WIN32)
    target_link_libraries(mfd_api ${MFD_API_DEPENDENCY_VISIBILITY} ws2_32)
endif()

if(MFD_API_BUILD_SHARED)
    target_compile_definitions(mfd_api
        PRIVATE
            MFD_API_EXPORTS
        PUBLIC
            MFD_API_DLL)
    mfd_stage_runtime(mfd_api WITH_RUNTIME_DLLS)
endif()

set(MFD_RENDER_PUBLIC_HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/include/mfd/render/MfdRenderer.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/mfd/render/OpenGlFramebufferReader.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/mfd/render/WindowBranding.h)

set(MFD_RENDER_PRIVATE_HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/render/BezierPolylineCache.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/render/Canvas2D.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/render/ImageTextureCache.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/render/OpenGlCompat.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/render/PolygonTriangulation.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/render/RenderWorkBudget.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/render/RenderTextureUtils.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/render/TextLayoutCache.h)

set(MFD_RENDER_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/render/BezierPolylineCache.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/render/Canvas2D.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/render/ImageTextureCache.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/render/MfdRenderer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/render/OpenGlCompat.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/render/OpenGlFramebufferReader.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/render/PolygonTriangulation.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/render/RenderTextureUtils.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/render/TextLayoutCache.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/render/WindowBranding.cpp)

# Host-side raylib renderer intentionally kept outside `mfd_api` so the DLL
# stays render-agnostic and can coexist with one single raylib instance in the
# executable hosting the OpenGL context.
add_library(mfd_render_raylib_objects OBJECT
    ${MFD_RENDER_PUBLIC_HEADERS}
    ${MFD_RENDER_PRIVATE_HEADERS}
    ${MFD_RENDER_SOURCES})

mfd_configure_component(
    mfd_render_raylib_objects
    PUBLIC_INCLUDE_DIRS
        "${MFD_COMMON_API_PUBLIC_INCLUDE_DIR}"
        "${MFD_API_PUBLIC_INCLUDE_DIR}")
target_include_directories(mfd_render_raylib_objects PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/render)
target_link_libraries(mfd_render_raylib_objects
    PRIVATE
        mfd::model
        mfd::runtime
        raylib)

if(WIN32)
    target_link_libraries(mfd_render_raylib_objects PUBLIC opengl32)
endif()

add_library(mfd_render_raylib STATIC
    ${MFD_RENDER_PUBLIC_HEADERS}
    ${MFD_RENDER_PRIVATE_HEADERS}
    $<TARGET_OBJECTS:mfd_render_raylib_objects>)
add_library(mfd::render_raylib ALIAS mfd_render_raylib)

mfd_configure_component(
    mfd_render_raylib
    PUBLIC_INCLUDE_DIRS
        "${MFD_COMMON_API_PUBLIC_INCLUDE_DIR}"
        "${MFD_API_PUBLIC_INCLUDE_DIR}")
target_include_directories(mfd_render_raylib PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/render)
target_link_libraries(mfd_render_raylib
    PUBLIC
        raylib)

if(WIN32)
    target_link_libraries(mfd_render_raylib PUBLIC opengl32)
endif()

install(TARGETS mfd_api
    ARCHIVE DESTINATION "${MFD_PACKAGE_LIB_DESTINATION}" COMPONENT ${MFD_COMPONENT_API_DEVELOPMENT}
    LIBRARY DESTINATION "${MFD_PACKAGE_LIB_DESTINATION}" COMPONENT ${MFD_COMPONENT_API_DEVELOPMENT}
    RUNTIME DESTINATION . COMPONENT ${MFD_COMPONENT_API_RUNTIME})

install(FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/include/mfd/control/UserSpaceProjector.h
    DESTINATION include/mfd/control
    COMPONENT ${MFD_COMPONENT_API_DEVELOPMENT})
install(FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/include/mfd/runtime/DocumentSemanticValidator.h
    DESTINATION include/mfd/runtime
    COMPONENT ${MFD_COMPONENT_API_DEVELOPMENT})
install(FILES
    ${MFD_IO_JSON_PUBLIC_HEADERS}
    DESTINATION include/mfd/io
    COMPONENT ${MFD_COMPONENT_API_DEVELOPMENT})
