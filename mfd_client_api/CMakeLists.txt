# This file is part of MFDStudio.
# Project author: Benoit Fra
# Repository: https://github.com/benoitfragit/MFDStudio
set(CLIENT_API_PUBLIC_HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/include/mfd/client/Animation.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/mfd/client/LatestBatchPublisher.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/mfd/client/ClientExport.h)

set(CLIENT_API_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Animation.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/LatestBatchPublisher.cpp)

add_library(mfd_client_api SHARED
    ${CLIENT_API_PUBLIC_HEADERS}
    ${CLIENT_API_SOURCES})

add_library(mfd::client ALIAS mfd_client_api)

target_include_directories(mfd_client_api
    PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
        "$<INSTALL_INTERFACE:include>")

target_link_libraries(mfd_client_api
    PUBLIC
        mfd::public_api)

target_compile_definitions(mfd_client_api
    PUBLIC
        MFD_CLIENT_API_DLL
    PRIVATE
        MFD_CLIENT_API_EXPORTS)

if(WIN32)
    target_compile_definitions(mfd_client_api PUBLIC NOMINMAX WIN32_LEAN_AND_MEAN)
endif()

mfd_enable_target_warnings(mfd_client_api)

mfd_stage_runtime(mfd_client_api WITH_RUNTIME_DLLS)

mfd_register_dev_package(
    NAME MFDStudioClientApi
    TARGET mfd_client_api
    ADDITIONAL_LIBRARY_TARGETS
        mfd_public_api
    IMPORTED_TARGET_NAME MFDStudio::ClientApi
    PUBLIC_INCLUDE_DIRS
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
        "${MFD_ROOT_DIR}/mfd_api/include"
    PUBLIC_HEADER_FILES
        "${CMAKE_CURRENT_SOURCE_DIR}/include/mfd/client/Animation.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/include/mfd/client/ClientExport.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/include/mfd/client/LatestBatchPublisher.h"
        "${MFD_ROOT_DIR}/mfd_api/include/mfd/MfdExport.h"
        "${MFD_ROOT_DIR}/mfd_api/include/mfd/core/ArrayView.h"
        "${MFD_ROOT_DIR}/mfd_api/include/mfd/control/CommandClient.h"
        "${MFD_ROOT_DIR}/mfd_api/include/mfd/control/CommandTransport.h"
        "${MFD_ROOT_DIR}/mfd_api/include/mfd/control/CommandTypes.h"
        "${MFD_ROOT_DIR}/mfd_api/include/mfd/ipc/ExchangeChannel.h"
        "${MFD_ROOT_DIR}/mfd_api/include/mfd/ipc/UdpLimits.h"
        "${MFD_ROOT_DIR}/mfd_api/include/mfd/model/PageDefinition.h"
        "${MFD_ROOT_DIR}/mfd_api/include/mfd/model/PageName.h"
        "${MFD_ROOT_DIR}/mfd_api/include/mfd/model/Reticle.h"
        "${MFD_ROOT_DIR}/mfd_api/include/mfd/model/Types.h"
        "${MFD_ROOT_DIR}/mfd_api/include/mfd/runtime/GeneratedTransportMap.h"
    PACKAGE_CMAKE_FILES
        "${MFD_ROOT_DIR}/mfd_client_api/generator/ClientApiGenerator.cmake"
    TOOL_DIRECTORIES
        "${MFD_ROOT_DIR}/mfd_client_api/generator"
    CONFIG_TEMPLATE
        "${MFD_ROOT_DIR}/cmake/packages/MFDStudioClientApiConfig.cmake.in")

mfd_register_runtime_package(
    NAME MFDStudioClientApi.Install
    TARGETS mfd_client_api)
