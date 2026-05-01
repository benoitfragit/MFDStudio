# This file is part of MFDStudio.
# Project author: Benoit Fra
# Repository: https://github.com/benoitfragit/MFDStudio
set(MFD_API_PUBLIC_HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/include/mfd/MfdExport.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/mfd/core/ArrayView.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/mfd/control/CommandClient.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/mfd/control/CommandProcessor.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/mfd/control/CommandTransport.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/mfd/control/CommandTypes.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/mfd/control/FeedbackTransport.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/mfd/control/StrobeFeedback.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/mfd/control/UserSpaceProjector.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/mfd/control/UdpRuntimeBridge.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/mfd/io/JsonLoader.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/mfd/ipc/ExchangeChannel.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/mfd/ipc/UdpChannel.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/mfd/ipc/UdpLimits.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/mfd/model/PageDefinition.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/mfd/model/PageName.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/mfd/model/Reticle.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/mfd/model/Types.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/mfd/render/OpenGlFramebufferReader.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/mfd/render/MfdRenderer.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/mfd/render/WindowBranding.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/mfd/runtime/GeneratedTransportMap.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/mfd/runtime/SceneRegistry.h)

set(MFD_API_PRIVATE_HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/io/json/JsonNumberParsing.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/io/json/JsonValueHelpers.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/render/Canvas2D.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/render/ImageTextureCache.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/render/RenderTextureUtils.h)

set(MFD_API_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/control/CommandClient.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/control/CommandProcessor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/control/CommandTransport.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/control/CommandTypes.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/control/FeedbackTransport.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/control/StrobeFeedback.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/control/UserSpaceProjector.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/control/UdpRuntimeBridge.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/io/JsonLoader.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/io/json/JsonNumberParsing.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/io/json/JsonValueHelpers.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ipc/UdpChannel.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/model/PageName.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/model/Reticle.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/model/Types.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/render/Canvas2D.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/render/ImageTextureCache.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/render/MfdRenderer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/render/OpenGlCompat.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/render/OpenGlFramebufferReader.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/render/RenderTextureUtils.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/render/WindowBranding.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/runtime/GeneratedTransportMap.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/runtime/SceneRegistry.cpp)

set(MFD_API_PROTO_OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated)
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

add_library(mfd_api SHARED
    ${MFD_API_PUBLIC_HEADERS}
    ${MFD_API_PRIVATE_HEADERS}
    ${MFD_API_SOURCES}
    ${MFD_API_PROTO_HEADERS}
    ${MFD_API_PROTO_SOURCES})
add_library(mfd::api ALIAS mfd_api)
add_library(mfd::core ALIAS mfd_api)

target_include_directories(mfd_api
    PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
        "$<INSTALL_INTERFACE:include>")
target_include_directories(mfd_api PRIVATE ${MFD_API_PROTO_OUTPUT_DIR})

mfd_begin_external_libraries(mfd_api)
mfd_public_library(mfd_api EnTT::EnTT)
mfd_public_library(mfd_api nlohmann_json::nlohmann_json)
mfd_private_library(mfd_api raylib)
mfd_private_library(mfd_api protobuf::libprotobuf)
mfd_end_external_libraries(mfd_api)

target_compile_definitions(mfd_api
    PUBLIC
        MFD_API_DLL
    PRIVATE
        MFD_API_EXPORTS)

if(WIN32)
    target_link_libraries(mfd_api PUBLIC ws2_32 opengl32)
    target_compile_definitions(mfd_api PUBLIC NOMINMAX WIN32_LEAN_AND_MEAN)
endif()

if(MFD_ENABLE_WARNINGS)
    if(MSVC)
        target_compile_options(mfd_api
            PRIVATE
                /W4
                /permissive-
                /Zc:__cplusplus
                /EHsc
                /wd4251
                /wd4275)
    else()
        target_compile_options(mfd_api PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endif()

mfd_stage_runtime(mfd_api WITH_RUNTIME_DLLS)
