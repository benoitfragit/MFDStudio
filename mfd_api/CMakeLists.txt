# This file is part of MFDStudio.
# Project author: Benoit Fra
# Repository: https://github.com/benoitfragit/MFDStudio
set(MFD_API_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include")
set(MFD_API_PROTO_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
set(MFD_API_PROTO_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/proto/mfd_commands.proto
    ${CMAKE_CURRENT_SOURCE_DIR}/proto/mfd_feedback.proto)

set(MFD_API_PROTO_HEADERS)
set(MFD_API_PROTO_SOURCES)
set(MFD_API_PROTOC_ENV_ARGS
    --modify
    "PATH=path_list_prepend:$<TARGET_FILE_DIR:protobuf::protoc>")
set(MFD_API_PROTO_DEPENDS protobuf::protoc)

if(TARGET abseil_dll)
    list(APPEND MFD_API_PROTOC_ENV_ARGS
        --modify
        "PATH=path_list_prepend:$<TARGET_FILE_DIR:abseil_dll>")
    list(APPEND MFD_API_PROTO_DEPENDS abseil_dll)
endif()

if(TARGET utf8_validity)
    list(APPEND MFD_API_PROTOC_ENV_ARGS
        --modify
        "PATH=path_list_prepend:$<TARGET_FILE_DIR:utf8_validity>")
    list(APPEND MFD_API_PROTO_DEPENDS utf8_validity)
endif()

foreach(MFD_API_PROTO_FILE IN LISTS MFD_API_PROTO_FILES)
    get_filename_component(MFD_API_PROTO_NAME ${MFD_API_PROTO_FILE} NAME_WE)
    set(MFD_API_PROTO_HEADER ${MFD_API_PROTO_OUTPUT_DIR}/${MFD_API_PROTO_NAME}.pb.h)
    set(MFD_API_PROTO_SOURCE ${MFD_API_PROTO_OUTPUT_DIR}/${MFD_API_PROTO_NAME}.pb.cc)

    add_custom_command(
        OUTPUT
            ${MFD_API_PROTO_HEADER}
            ${MFD_API_PROTO_SOURCE}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${MFD_API_PROTO_OUTPUT_DIR}
        COMMAND
            ${CMAKE_COMMAND} -E env
            ${MFD_API_PROTOC_ENV_ARGS}
            $<TARGET_FILE:protobuf::protoc>
            --cpp_out=${MFD_API_PROTO_OUTPUT_DIR}
            --proto_path=${CMAKE_CURRENT_SOURCE_DIR}/proto
            ${MFD_API_PROTO_FILE}
        DEPENDS
            ${MFD_API_PROTO_FILE}
            ${CMAKE_CURRENT_SOURCE_DIR}/proto/mfd_commands.proto
            ${MFD_API_PROTO_DEPENDS}
        COMMAND_EXPAND_LISTS
        VERBATIM)

    list(APPEND MFD_API_PROTO_HEADERS ${MFD_API_PROTO_HEADER})
    list(APPEND MFD_API_PROTO_SOURCES ${MFD_API_PROTO_SOURCE})
endforeach()

function(mfd_configure_component target_name)
    target_include_directories(${target_name}
        PUBLIC
            "$<BUILD_INTERFACE:${MFD_API_INCLUDE_DIR}>"
            "$<INSTALL_INTERFACE:include>")

    if(WIN32)
        target_compile_definitions(${target_name} PUBLIC NOMINMAX WIN32_LEAN_AND_MEAN)
    endif()

    mfd_enable_target_warnings(${target_name})
endfunction()

set(MFD_MODEL_PUBLIC_HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/include/mfd/MfdExport.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/mfd/core/ArrayView.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/mfd/model/PageDefinition.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/mfd/model/PageName.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/mfd/model/Reticle.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/mfd/model/Types.h)

set(MFD_MODEL_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/model/PageName.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/model/Reticle.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/model/Types.cpp)

add_library(mfd_model STATIC
    ${MFD_MODEL_PUBLIC_HEADERS}
    ${MFD_MODEL_SOURCES})
add_library(mfd::model ALIAS mfd_model)

mfd_configure_component(mfd_model)

set(MFD_TRANSPORT_PUBLIC_HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/include/mfd/control/CommandClient.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/mfd/control/CommandTransport.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/mfd/control/CommandTypes.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/mfd/control/FeedbackTransport.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/mfd/control/StrobeFeedback.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/mfd/ipc/ExchangeChannel.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/mfd/ipc/UdpChannel.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/mfd/ipc/UdpLimits.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/mfd/runtime/GeneratedTransportMap.h)

set(MFD_TRANSPORT_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/control/CommandClient.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/control/CommandTransport.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/control/CommandTypes.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/control/FeedbackTransport.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/control/StrobeFeedback.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ipc/UdpChannel.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/runtime/GeneratedTransportMap.cpp
    ${MFD_API_PROTO_HEADERS}
    ${MFD_API_PROTO_SOURCES})

add_library(mfd_transport STATIC
    ${MFD_TRANSPORT_PUBLIC_HEADERS}
    ${MFD_TRANSPORT_SOURCES})
add_library(mfd::transport ALIAS mfd_transport)

mfd_configure_component(mfd_transport)
target_include_directories(mfd_transport PRIVATE ${MFD_API_PROTO_OUTPUT_DIR})
target_link_libraries(mfd_transport
    PUBLIC
        mfd::model
    PRIVATE
        nlohmann_json::nlohmann_json
        protobuf::libprotobuf)

if(WIN32)
    target_link_libraries(mfd_transport PRIVATE ws2_32)
endif()

set(MFD_PUBLIC_API_PUBLIC_HEADERS
    ${MFD_MODEL_PUBLIC_HEADERS}
    ${MFD_TRANSPORT_PUBLIC_HEADERS})

set(MFD_PUBLIC_API_SOURCES
    ${MFD_MODEL_SOURCES}
    ${MFD_TRANSPORT_SOURCES})

add_library(mfd_public_api SHARED
    ${MFD_PUBLIC_API_PUBLIC_HEADERS}
    ${MFD_PUBLIC_API_SOURCES})
add_library(mfd::public_api ALIAS mfd_public_api)

mfd_configure_component(mfd_public_api)
target_include_directories(mfd_public_api PRIVATE ${MFD_API_PROTO_OUTPUT_DIR})
target_compile_definitions(mfd_public_api
    PRIVATE
        MFD_API_EXPORTS)
set_target_properties(mfd_public_api PROPERTIES OUTPUT_NAME "mfd_api")
target_link_libraries(mfd_public_api
    PRIVATE
        nlohmann_json::nlohmann_json
        protobuf::libprotobuf)

if(WIN32)
    add_custom_command(TARGET mfd_public_api POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E rm -f
            "$<TARGET_FILE_DIR:mfd_public_api>/mfd_public_api.dll"
            "$<TARGET_FILE_DIR:mfd_public_api>/mfd_public_api.exp"
            "$<TARGET_LINKER_FILE_DIR:mfd_public_api>/mfd_public_api.lib"
            "$<TARGET_FILE_DIR:mfd_public_api>/mfd_public_api.pdb"
        VERBATIM)
endif()

if(WIN32)
    target_link_libraries(mfd_public_api PRIVATE ws2_32)
endif()

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

mfd_configure_component(mfd_runtime)
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

mfd_configure_component(mfd_io_json)
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

mfd_configure_component(mfd_render_raylib)
target_include_directories(mfd_render_raylib PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/render)
target_link_libraries(mfd_render_raylib
    PUBLIC
        mfd::model
        mfd::runtime
        raylib)

if(WIN32)
    target_link_libraries(mfd_render_raylib PUBLIC opengl32)
endif()

add_library(mfd_api INTERFACE)
add_library(mfd::api ALIAS mfd_api)
add_library(mfd::core ALIAS mfd_api)

target_link_libraries(mfd_api
    INTERFACE
        mfd::io_json
        mfd::model
        mfd::runtime
        mfd::transport)
