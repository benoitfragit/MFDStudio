# This file is part of MFDStudio.
# Project author: Benoit Fra
# Repository: https://github.com/benoitfragit/MFDStudio
cmake_minimum_required(VERSION 3.26)

project(MFD
    VERSION 0.1.0
    DESCRIPTION "2D Multi-Function Display framework with JSON pages and reusable reticles"
    LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(MFD_BUILD_DEMO "Build the demo MFD executable" ON)
option(MFD_BUILD_TESTS "Build GoogleTest-based runtime and loading tests" ON)
option(MFD_ENABLE_WARNINGS "Enable strict compiler warnings" ON)

if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

include(Dependencies)

mfd_fetch_dependencies()

set(MFD_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}")

include(RuntimePackaging)

add_subdirectory(mfd_api)

if(MFD_BUILD_TESTS)
    enable_testing()
    add_subdirectory(mfd_api/tests)
endif()

if(MFD_BUILD_DEMO)
    add_subdirectory(examples/mfd_demo)
    add_subdirectory(examples/mfd_demo_cockpit)
    add_subdirectory(examples/mfd_demo_minimal)
    add_subdirectory(examples/mfd_mockup_minimal)
    add_subdirectory(mfd_mockup)
    add_subdirectory(mfd_editor)
endif()
