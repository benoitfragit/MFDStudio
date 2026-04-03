# This file is part of MFDStudio.
# Project author: Benoit Fra
# Repository: https://github.com/benoitfragit/MFDStudio
set(CLIENT_API_PUBLIC_HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/include/mfd/client/Animation.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/mfd/client/ClientExport.h)

set(CLIENT_API_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Animation.cpp)

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
        mfd::api)

target_compile_definitions(client_api
    PUBLIC
        MFD_CLIENT_API_DLL
    PRIVATE
        MFD_CLIENT_API_EXPORTS)

if(WIN32)
    target_compile_definitions(client_api PUBLIC NOMINMAX WIN32_LEAN_AND_MEAN)
endif()

if(MSVC)
    target_compile_options(client_api
        PRIVATE
            /W4
            /permissive-
            /Zc:__cplusplus
            /EHsc
            /wd4251
            /wd4275)
else()
    target_compile_options(client_api PRIVATE -Wall -Wextra -Wpedantic)
endif()

mfd_stage_runtime(client_api WITH_RUNTIME_DLLS)
