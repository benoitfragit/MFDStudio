# This file is part of MFDStudio.
# Project author: Benoit Fra
# Repository: https://github.com/benoitfragit/MFDStudio
cmake_minimum_required(VERSION 3.26)

project(MFD
    VERSION 0.1.0
    DESCRIPTION "2D Multi-Function Display framework with JSON pages and reusable reticles"
    LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(MFD_BUILD_DEMO "Build the demo MFD executable" ON)
option(MFD_BUILD_TESTS "Build GoogleTest-based runtime and loading tests" ON)
option(MFD_ENABLE_WARNINGS "Enable strict compiler warnings" ON)

set(MFD_ENABLE_POSITION_INDEPENDENT_CODE_DEFAULT ON)
if(WIN32)
    set(MFD_ENABLE_POSITION_INDEPENDENT_CODE_DEFAULT OFF)
endif()

option(MFD_ENABLE_POSITION_INDEPENDENT_CODE
    "Enable position-independent code for targets that need it"
    ${MFD_ENABLE_POSITION_INDEPENDENT_CODE_DEFAULT})

set(MFD_MSVC_RUNTIME "default" CACHE STRING
    "MSVC C runtime linkage. Supported values: default, dll, static")
set_property(CACHE MFD_MSVC_RUNTIME PROPERTY STRINGS default dll static)

if(MFD_ENABLE_POSITION_INDEPENDENT_CODE)
    set(CMAKE_POSITION_INDEPENDENT_CODE ON)
endif()

if(MSVC)
    string(TOLOWER "${MFD_MSVC_RUNTIME}" mfd_msvc_runtime)
    if(mfd_msvc_runtime STREQUAL "default")
        unset(CMAKE_MSVC_RUNTIME_LIBRARY)
        unset(CMAKE_MSVC_RUNTIME_LIBRARY CACHE)
        set(MFD_MSVC_USE_DLL_RUNTIME ON)
    elseif(mfd_msvc_runtime STREQUAL "dll")
        set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
        set(MFD_MSVC_USE_DLL_RUNTIME ON)
    elseif(mfd_msvc_runtime STREQUAL "static")
        set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
        set(MFD_MSVC_USE_DLL_RUNTIME OFF)
    else()
        message(FATAL_ERROR
            "Unsupported MFD_MSVC_RUNTIME='${MFD_MSVC_RUNTIME}'. "
            "Expected one of: default, dll, static.")
    endif()

    set(MFD_MSVC_USE_DLL_RUNTIME "${MFD_MSVC_USE_DLL_RUNTIME}" CACHE INTERNAL
        "Internal helper controlling third-party MSVC runtime alignment" FORCE)
    add_compile_options(/FS)
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

include(Dependencies)

mfd_fetch_dependencies()

set(MFD_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}")

include(RuntimePackaging)

add_subdirectory(mfd_api)
add_subdirectory(client_api)
add_subdirectory(client_api_generator)
add_subdirectory(mfd_window)

include(${CMAKE_CURRENT_SOURCE_DIR}/client_api_generator/ClientApiGenerator.cmake)

if(MFD_BUILD_TESTS)
    enable_testing()
    add_subdirectory(mfd_api/tests)
    add_subdirectory(client_api/tests)
    add_subdirectory(client_api_generator/tests)
    add_subdirectory(mfd_window/tests)
    add_subdirectory(mfd_editor/tests)
endif()

if(MFD_BUILD_DEMO)
    add_subdirectory(examples/mfd_framebuffer_stdout_plugin)
    add_subdirectory(examples/client_mockup)
    add_subdirectory(examples/client_mockup_minimal)
    add_subdirectory(mfd_editor)
endif()
