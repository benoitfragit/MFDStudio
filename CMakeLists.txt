# This file is part of MFDStudio.
# Project author: Benoit Fra
# Repository: https://github.com/benoitfragit/MFDStudio
cmake_minimum_required(VERSION 3.25)

project(MFD
    VERSION 1.3.0
    DESCRIPTION "2D Multi-Function Display framework with JSON pages and reusable reticles"
    LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(MFD_BUILD_DEMO "Build the demo MFD executable" ON)
option(MFD_BUILD_TESTS "Build GoogleTest-based runtime and loading tests" ON)
option(MFD_ENABLE_WARNINGS "Enable strict compiler warnings" ON)
option(USE_LOCALE_PACKAGE "When ON, FetchContent expects local source archives under MFD_LOCAL_PACKAGE_ROOT instead of using the remote Git declarations." OFF)

set(MFD_LOCAL_PACKAGE_ROOT
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/archives"
    CACHE PATH
    "Root directory containing local third-party source archives grouped by dependency name.")

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

include(ExternalLibraries)
include(CompilerWarnings)
include(Dependencies)

mfd_fetch_dependencies()

set(MFD_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}")

include(RuntimePackaging)
include(DeliveryPackaging)

if(MFD_BUILD_TESTS)
    enable_testing()
endif()

add_subdirectory(mfd_api)
add_subdirectory(mfd_client_api/generator)

include(${CMAKE_CURRENT_SOURCE_DIR}/mfd_client_api/generator/ClientApiGenerator.cmake)

add_subdirectory(mfd_client_api)
add_subdirectory(mfd_window_plugin_api)
add_subdirectory(mfd_window)
add_subdirectory(mfd_editor_plugin_api)
add_subdirectory(mfd_editor)

if(MFD_BUILD_TESTS)
    add_subdirectory(tests)
endif()

if(MFD_BUILD_DEMO)
    add_subdirectory(examples)
endif()

mfd_register_runtime_package(
    NAME MFDStudioWindowLauncher.Install
    TARGETS mfd_window
    WITH_BRANDING
    SCRIPT_SOURCE_DIR "${MFD_ROOT_DIR}/Scripts"
    SCRIPT_NAMES
        Start-MfdWindow.bat)

mfd_finalize_delivery_targets()
