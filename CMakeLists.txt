# This file is part of MFDStudio.
# Project author: Benoit Fra
# Repository: https://github.com/benoitfragit/MFDStudio
cmake_minimum_required(VERSION 3.25)

project(MFD
    VERSION 2.2.2
    DESCRIPTION "2D Multi-Function Display framework with JSON pages and reusable reticles"
    LANGUAGES C CXX)

include(CMakePackageConfigHelpers)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(MFD_BUILD_DEMO "Build optional example and demo applications" ON)
option(MFD_BUILD_TESTS "Build GoogleTest-based runtime and loading tests" ON)
option(MFD_ENABLE_WARNINGS "Enable strict compiler warnings" ON)
option(MFD_ENABLE_WARNINGS_AS_ERRORS "Treat repository compiler warnings as errors" OFF)
option(MFD_ENABLE_FUZZING "Build dedicated libFuzzer targets in one external Clang build" OFF)
option(MFD_API_BUILD_SHARED "Build mfd_api as one self-contained shared library" ON)

set(MFD_PACKAGE_VERSION
    "${PROJECT_VERSION}"
    CACHE STRING
    "Version string written into generated delivery package metadata.")

if(MFD_PACKAGE_VERSION STREQUAL "")
    message(FATAL_ERROR "MFD_PACKAGE_VERSION must not be empty.")
endif()

set(_mfd_use_local_package_is_defined OFF)
set(_mfd_use_local_package_value OFF)
if(DEFINED CACHE{USE_LOCAL_PACKAGE})
    set(_mfd_use_local_package_is_defined ON)
    set(_mfd_use_local_package_value "${USE_LOCAL_PACKAGE}")
endif()

set(_mfd_use_locale_package_is_defined OFF)
set(_mfd_use_locale_package_value OFF)
if(DEFINED CACHE{USE_LOCALE_PACKAGE})
    set(_mfd_use_locale_package_is_defined ON)
    set(_mfd_use_locale_package_value "${USE_LOCALE_PACKAGE}")
endif()

set(_mfd_use_local_package_default OFF)
if(_mfd_use_local_package_is_defined)
    set(_mfd_use_local_package_default "${_mfd_use_local_package_value}")
elseif(_mfd_use_locale_package_is_defined)
    set(_mfd_use_local_package_default "${_mfd_use_locale_package_value}")
endif()

option(USE_LOCAL_PACKAGE
    "When ON, FetchContent expects local source archives under MFD_LOCAL_PACKAGE_ROOT instead of using the remote Git declarations."
    ${_mfd_use_local_package_default})

if(_mfd_use_locale_package_is_defined)
    if(_mfd_use_local_package_is_defined AND
       NOT "${_mfd_use_local_package_value}" STREQUAL "${_mfd_use_locale_package_value}")
        message(WARNING
            "Both USE_LOCAL_PACKAGE='${_mfd_use_local_package_value}' and deprecated "
            "USE_LOCALE_PACKAGE='${_mfd_use_locale_package_value}' are defined. "
            "USE_LOCAL_PACKAGE takes precedence.")
    elseif(NOT _mfd_use_local_package_is_defined)
        message(WARNING
            "USE_LOCALE_PACKAGE is deprecated and will be removed in a future release. "
            "Use USE_LOCAL_PACKAGE instead.")
    endif()
endif()

set(USE_LOCALE_PACKAGE "${USE_LOCAL_PACKAGE}" CACHE BOOL "Deprecated alias for USE_LOCAL_PACKAGE." FORCE)
mark_as_advanced(USE_LOCALE_PACKAGE)

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
include(Fuzzing)
include(Dependencies)

mfd_fetch_dependencies()

set(MFD_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
set(MFD_COMMON_API_PUBLIC_INCLUDE_DIR "${MFD_ROOT_DIR}/mfd_common_api/include")
set(MFD_API_PUBLIC_INCLUDE_DIR "${MFD_ROOT_DIR}/mfd_api/include")

include(RuntimePackaging)

set(MFD_PACKAGE_TOOLSET "native")
if(MSVC)
    if(DEFINED CMAKE_VS_PLATFORM_TOOLSET AND NOT CMAKE_VS_PLATFORM_TOOLSET STREQUAL "")
        set(MFD_PACKAGE_TOOLSET "${CMAKE_VS_PLATFORM_TOOLSET}")
    elseif(DEFINED MSVC_TOOLSET_VERSION AND NOT MSVC_TOOLSET_VERSION STREQUAL "")
        set(MFD_PACKAGE_TOOLSET "v${MSVC_TOOLSET_VERSION}")
    endif()
endif()

set(MFD_PACKAGE_ARCH "${MFD_EXEC_PLATFORM_DIR}")
set(MFD_PACKAGE_LIB_DESTINATION "libs/${MFD_PACKAGE_TOOLSET}/${MFD_PACKAGE_ARCH}/$<CONFIG>")

set(MFD_COMPONENT_API_DEVELOPMENT mfd_api_development)
set(MFD_COMPONENT_API_RUNTIME mfd_api_runtime)
set(MFD_COMPONENT_RUNTIME_API_DEVELOPMENT mfd_runtime_api_development)
set(MFD_COMPONENT_RUNTIME_API_RUNTIME mfd_runtime_api_runtime)
set(MFD_COMPONENT_CLIENT_API_DEVELOPMENT mfd_client_api_development)
set(MFD_COMPONENT_CLIENT_API_RUNTIME mfd_client_api_runtime)
set(MFD_COMPONENT_WINDOW_PLUGIN_DEVELOPMENT mfd_window_plugin_api_development)
set(MFD_COMPONENT_WINDOW_LAUNCHER_RUNTIME mfd_window_launcher_runtime)
set(MFD_COMPONENT_EDITOR_RUNTIME mfd_editor_runtime)

if(MFD_BUILD_TESTS)
    enable_testing()
endif()

add_subdirectory(mfd_common_api)
add_subdirectory(mfd_api)
add_subdirectory(mfd_runtime_api)
add_subdirectory(mfd_client_api/generator)

include(${CMAKE_CURRENT_SOURCE_DIR}/mfd_client_api/generator/ClientApiGenerator.cmake)

add_subdirectory(mfd_client_api)
add_subdirectory(mfd_window_plugin_api)
add_subdirectory(mfd_window)
add_subdirectory(mfd_editor)

if(MFD_BUILD_TESTS)
    add_subdirectory(tests)
endif()

if(MFD_ENABLE_FUZZING)
    add_subdirectory(tests/fuzz)
endif()

if(MFD_BUILD_DEMO)
    add_subdirectory(examples)
endif()

mfd_finalize_runtime_stage_targets()
