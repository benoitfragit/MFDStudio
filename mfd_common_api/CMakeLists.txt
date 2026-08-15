# This file is part of MFDStudio.
# Project author: Benoit Fra
# Repository: https://github.com/benoitfragit/MFDStudio

set(MFD_COMMON_API_PROTO_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
set(MFD_COMMON_API_PROTO_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/proto/mfd_commands.proto
    ${CMAKE_CURRENT_SOURCE_DIR}/proto/mfd_feedback.proto)

set(MFD_COMMON_API_PROTO_HEADERS)
set(MFD_COMMON_API_PROTO_SOURCES)
set(MFD_COMMON_API_PROTOC_ENV_ARGS
    --modify
    "PATH=path_list_prepend:$<TARGET_FILE_DIR:protobuf::protoc>")
set(MFD_COMMON_API_PROTO_DEPENDS protobuf::protoc)

foreach(MFD_COMMON_API_PROTO_FILE IN LISTS MFD_COMMON_API_PROTO_FILES)
    get_filename_component(MFD_COMMON_API_PROTO_NAME ${MFD_COMMON_API_PROTO_FILE} NAME_WE)
    set(MFD_COMMON_API_PROTO_HEADER
        ${MFD_COMMON_API_PROTO_OUTPUT_DIR}/${MFD_COMMON_API_PROTO_NAME}.pb.h)
    set(MFD_COMMON_API_PROTO_SOURCE
        ${MFD_COMMON_API_PROTO_OUTPUT_DIR}/${MFD_COMMON_API_PROTO_NAME}.pb.cc)

    add_custom_command(
        OUTPUT
            ${MFD_COMMON_API_PROTO_HEADER}
            ${MFD_COMMON_API_PROTO_SOURCE}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${MFD_COMMON_API_PROTO_OUTPUT_DIR}
        COMMAND
            ${CMAKE_COMMAND} -E env
            ${MFD_COMMON_API_PROTOC_ENV_ARGS}
            $<TARGET_FILE:protobuf::protoc>
            --cpp_out=${MFD_COMMON_API_PROTO_OUTPUT_DIR}
            --proto_path=${CMAKE_CURRENT_SOURCE_DIR}/proto
            ${MFD_COMMON_API_PROTO_FILE}
        DEPENDS
            ${MFD_COMMON_API_PROTO_FILE}
            ${CMAKE_CURRENT_SOURCE_DIR}/proto/mfd_commands.proto
            ${MFD_COMMON_API_PROTO_DEPENDS}
        COMMAND_EXPAND_LISTS
        VERBATIM)

    list(APPEND MFD_COMMON_API_PROTO_HEADERS ${MFD_COMMON_API_PROTO_HEADER})
    list(APPEND MFD_COMMON_API_PROTO_SOURCES ${MFD_COMMON_API_PROTO_SOURCE})
endforeach()

function(mfd_configure_component target_name)
    set(options)
    set(oneValueArgs)
    set(multiValueArgs PUBLIC_INCLUDE_DIRS)
    cmake_parse_arguments(MFD_COMPONENT "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    if(NOT MFD_COMPONENT_PUBLIC_INCLUDE_DIRS)
        set(MFD_COMPONENT_PUBLIC_INCLUDE_DIRS "${MFD_COMMON_API_PUBLIC_INCLUDE_DIR}")
    endif()

    set(mfd_component_build_include_dirs)
    foreach(mfd_component_include_dir IN LISTS MFD_COMPONENT_PUBLIC_INCLUDE_DIRS)
        list(APPEND mfd_component_build_include_dirs
            "$<BUILD_INTERFACE:${mfd_component_include_dir}>")
    endforeach()

    target_include_directories(${target_name}
        PUBLIC
            ${mfd_component_build_include_dirs}
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
    ${CMAKE_CURRENT_SOURCE_DIR}/include/mfd/model/RuntimeBudgets.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/mfd/model/Types.h)

set(MFD_MODEL_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/model/PageDefinition.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/model/PageName.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/model/Reticle.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/model/Types.cpp)

add_library(mfd_model_objects OBJECT
    ${MFD_MODEL_PUBLIC_HEADERS}
    ${MFD_MODEL_SOURCES})

mfd_configure_component(
    mfd_model_objects
    PUBLIC_INCLUDE_DIRS
        "${MFD_COMMON_API_PUBLIC_INCLUDE_DIR}")
if(MFD_ENABLE_FUZZING)
    mfd_enable_fuzzing_instrumentation(mfd_model_objects)
endif()
if(MFD_API_BUILD_SHARED)
    target_compile_definitions(mfd_model_objects PRIVATE MFD_API_EXPORTS)
endif()

add_library(mfd_model STATIC
    ${MFD_MODEL_PUBLIC_HEADERS}
    $<TARGET_OBJECTS:mfd_model_objects>)
add_library(mfd::model ALIAS mfd_model)

mfd_configure_component(
    mfd_model
    PUBLIC_INCLUDE_DIRS
        "${MFD_COMMON_API_PUBLIC_INCLUDE_DIR}")

set(MFD_TRANSPORT_PUBLIC_HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/include/mfd/control/CommandClient.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/mfd/control/CommandTransport.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/mfd/control/CommandTypes.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/mfd/control/FeedbackTransport.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/mfd/control/StrobeFeedback.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/mfd/control/WindowFeedback.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/mfd/ipc/ExchangeChannel.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/mfd/ipc/UdpChannel.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/mfd/ipc/UdpLimits.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/mfd/runtime/GeneratedTransportMap.h)

set(MFD_TRANSPORT_PRIVATE_HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/include/mfd/control/internal/CommandWorkBudget.h)

set(MFD_TRANSPORT_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/control/CommandClient.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/control/CommandTransport.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/control/CommandTypes.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/control/FeedbackTransport.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/control/StrobeFeedback.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/control/StrobeFeedbackProto.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/control/WindowFeedback.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ipc/UdpChannel.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/runtime/GeneratedTransportMap.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/runtime/Sha256.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/runtime/Sha256.h
    ${MFD_COMMON_API_PROTO_HEADERS}
    ${MFD_COMMON_API_PROTO_SOURCES})

add_library(mfd_transport_objects OBJECT
    ${MFD_TRANSPORT_PUBLIC_HEADERS}
    ${MFD_TRANSPORT_PRIVATE_HEADERS}
    ${MFD_TRANSPORT_SOURCES})

mfd_configure_component(
    mfd_transport_objects
    PUBLIC_INCLUDE_DIRS
        "${MFD_COMMON_API_PUBLIC_INCLUDE_DIR}")
if(MFD_ENABLE_FUZZING)
    mfd_enable_fuzzing_instrumentation(mfd_transport_objects)
endif()
if(MFD_API_BUILD_SHARED)
    target_compile_definitions(mfd_transport_objects PRIVATE MFD_API_EXPORTS)
endif()
target_include_directories(mfd_transport_objects PRIVATE ${MFD_COMMON_API_PROTO_OUTPUT_DIR})
target_link_libraries(mfd_transport_objects
    PUBLIC
        mfd::model
    PRIVATE
        nlohmann_json::nlohmann_json
        protobuf::libprotobuf)

if(WIN32)
    target_link_libraries(mfd_transport_objects PRIVATE ws2_32)
endif()

add_library(mfd_transport STATIC
    ${MFD_TRANSPORT_PUBLIC_HEADERS}
    ${MFD_TRANSPORT_PRIVATE_HEADERS}
    $<TARGET_OBJECTS:mfd_transport_objects>)
add_library(mfd::transport ALIAS mfd_transport)

mfd_configure_component(
    mfd_transport
    PUBLIC_INCLUDE_DIRS
        "${MFD_COMMON_API_PUBLIC_INCLUDE_DIR}")
target_link_libraries(mfd_transport
    PUBLIC
        mfd::model
    PRIVATE
        nlohmann_json::nlohmann_json
        protobuf::libprotobuf)

if(WIN32)
    target_link_libraries(mfd_transport PRIVATE ws2_32)
endif()
