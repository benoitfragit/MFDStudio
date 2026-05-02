# This file is part of MFDStudio.
# Project author: Benoit Fra
# Repository: https://github.com/benoitfragit/MFDStudio

set(MFD_RUNTIME_PUBLIC_HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/include/mfd/control/CommandProcessor.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/mfd/control/UdpRuntimeBridge.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/mfd/control/UserSpaceProjector.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/mfd/runtime/SceneRegistry.h)

set(MFD_RUNTIME_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/control/CommandProcessor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/control/UdpRuntimeBridge.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/control/UserSpaceProjector.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/runtime/SceneRegistry.cpp)

add_library(mfd_runtime STATIC
    ${MFD_RUNTIME_PUBLIC_HEADERS}
    ${MFD_RUNTIME_SOURCES})
add_library(mfd::runtime ALIAS mfd_runtime)

mfd_configure_component(
    mfd_runtime
    PUBLIC_INCLUDE_DIRS
        "${MFD_COMMON_API_PUBLIC_INCLUDE_DIR}"
        "${MFD_API_PUBLIC_INCLUDE_DIR}")
target_link_libraries(mfd_runtime
    PUBLIC
        EnTT::EnTT
        mfd::model
        mfd::transport)

set(MFD_IO_JSON_PUBLIC_HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/include/mfd/io/JsonLoader.h)

set(MFD_IO_JSON_PRIVATE_HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/io/json/JsonNumberParsing.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/io/json/JsonValueHelpers.h)

set(MFD_IO_JSON_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/io/JsonLoader.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/io/json/JsonNumberParsing.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/io/json/JsonValueHelpers.cpp)

add_library(mfd_io_json STATIC
    ${MFD_IO_JSON_PUBLIC_HEADERS}
    ${MFD_IO_JSON_PRIVATE_HEADERS}
    ${MFD_IO_JSON_SOURCES})
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

set(MFD_RENDER_PUBLIC_HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/include/mfd/render/MfdRenderer.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/mfd/render/OpenGlFramebufferReader.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/mfd/render/WindowBranding.h)

set(MFD_RENDER_PRIVATE_HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/render/Canvas2D.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/render/ImageTextureCache.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/render/OpenGlCompat.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/render/PolygonTriangulation.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/render/RenderTextureUtils.h)

set(MFD_RENDER_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/render/Canvas2D.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/render/ImageTextureCache.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/render/MfdRenderer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/render/OpenGlCompat.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/render/OpenGlFramebufferReader.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/render/PolygonTriangulation.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/render/RenderTextureUtils.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/render/WindowBranding.cpp)

add_library(mfd_render_raylib STATIC
    ${MFD_RENDER_PUBLIC_HEADERS}
    ${MFD_RENDER_PRIVATE_HEADERS}
    ${MFD_RENDER_SOURCES})
add_library(mfd::render_raylib ALIAS mfd_render_raylib)

mfd_configure_component(
    mfd_render_raylib
    PUBLIC_INCLUDE_DIRS
        "${MFD_COMMON_API_PUBLIC_INCLUDE_DIR}"
        "${MFD_API_PUBLIC_INCLUDE_DIR}")
target_include_directories(mfd_render_raylib PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/render)
target_link_libraries(mfd_render_raylib
    PUBLIC
        mfd::model
        mfd::runtime
        raylib)

if(WIN32)
    target_link_libraries(mfd_render_raylib PUBLIC opengl32)
endif()

add_library(mfd_api SHARED
    $<TARGET_OBJECTS:mfd_model_objects>
    $<TARGET_OBJECTS:mfd_transport_objects>)

mfd_configure_component(
    mfd_api
    PUBLIC_INCLUDE_DIRS
        "${MFD_COMMON_API_PUBLIC_INCLUDE_DIR}")
target_link_libraries(mfd_api
    PRIVATE
        nlohmann_json::nlohmann_json
        protobuf::libprotobuf)

set_target_properties(mfd_api PROPERTIES
    WINDOWS_EXPORT_ALL_SYMBOLS ON)

if(WIN32)
    target_link_libraries(mfd_api PRIVATE ws2_32)
endif()

mfd_stage_runtime(mfd_api WITH_RUNTIME_DLLS)

add_library(mfd_api_components INTERFACE)
add_library(mfd::api ALIAS mfd_api_components)
add_library(mfd::core ALIAS mfd_api_components)

target_link_libraries(mfd_api_components
    INTERFACE
        mfd::io_json
        mfd::model
        mfd::runtime
        mfd::transport)
