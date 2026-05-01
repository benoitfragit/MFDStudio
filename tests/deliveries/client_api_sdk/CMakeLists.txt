# This file is part of MFDStudio.
# Project author: Benoit Fra
# Repository: https://github.com/benoitfragit/MFDStudio
cmake_minimum_required(VERSION 3.25)

project(MFDStudioClientApiPackageSmoke LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT DEFINED MFD_PACKAGE_CMAKE_DIR OR MFD_PACKAGE_CMAKE_DIR STREQUAL "")
    message(FATAL_ERROR "MFD_PACKAGE_CMAKE_DIR must point to the staged MFDStudioClientApi package.")
endif()

if(NOT DEFINED MFD_SOURCE_DIR OR MFD_SOURCE_DIR STREQUAL "")
    message(FATAL_ERROR "MFD_SOURCE_DIR must point to the repository root.")
endif()

find_package(MFDStudioClientApi REQUIRED CONFIG
    PATHS "${MFD_PACKAGE_CMAKE_DIR}"
    NO_DEFAULT_PATH)

set(generated_dir "${CMAKE_CURRENT_BINARY_DIR}/generated")

client_api_generate_ui(
    WINDOW_JSON "${MFD_SOURCE_DIR}/tests/client_api/fixtures/generated_ui/window.json"
    OUTPUT_HEADER "${generated_dir}/PackageSmokeUi.h"
    OUTPUT_SOURCE "${generated_dir}/PackageSmokeUi.cpp"
    OUTPUT_MAP "${generated_dir}/PackageSmokeUi.generated.map"
    NAMESPACE "client_api_package_smoke"
    UI_CLASS_NAME "PackageSmokeUi"
    HEADER_INCLUDE "PackageSmokeUi.h")

add_library(client_api_package_smoke OBJECT
    "${generated_dir}/PackageSmokeUi.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Smoke.cpp")

target_include_directories(client_api_package_smoke
    PRIVATE
        "${generated_dir}")

target_link_libraries(client_api_package_smoke
    PRIVATE
        MFDStudio::ClientApi)
