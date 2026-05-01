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

add_library(client_api SHARED
    ${CLIENT_API_PUBLIC_HEADERS}
    ${CLIENT_API_SOURCES})

add_library(mfd::client ALIAS client_api)

target_include_directories(client_api
    PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
        "$<INSTALL_INTERFACE:include>")

target_link_libraries(client_api
    PUBLIC
        mfd::model
        mfd::transport)

target_compile_definitions(client_api
    PUBLIC
        MFD_CLIENT_API_DLL
    PRIVATE
        MFD_CLIENT_API_EXPORTS)

if(WIN32)
    target_compile_definitions(client_api PUBLIC NOMINMAX WIN32_LEAN_AND_MEAN)
endif()

mfd_enable_target_warnings(client_api)

mfd_stage_runtime(client_api WITH_RUNTIME_DLLS)
