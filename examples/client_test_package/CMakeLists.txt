# This file is part of MFDStudio.
# Project author: Benoit Fra
# Repository: https://github.com/benoitfragit/MFDStudio
set(MFD_CLIENT_TEST_PACKAGE_DELIVERY_ROOT
    "${MFD_ROOT_DIR}/_Deliveries")
set(MFD_CLIENT_TEST_PACKAGE_DEV_ROOT
    "${MFD_CLIENT_TEST_PACKAGE_DELIVERY_ROOT}/packages_windows/MFDStudioClientApi/build/native")
set(MFD_CLIENT_TEST_PACKAGE_CONFIG_FILE
    "${MFD_CLIENT_TEST_PACKAGE_DEV_ROOT}/cmake/MFDStudioClientApiConfig.cmake")
set(MFD_CLIENT_TEST_PACKAGE_DEBUG_IMPLIB
    "${MFD_CLIENT_TEST_PACKAGE_DEV_ROOT}/libs/${MFD_PACKAGE_TOOLSET}/${MFD_EXEC_PLATFORM_DIR}/Debug/mfd_client_api.lib")
set(MFD_CLIENT_TEST_PACKAGE_RELEASE_IMPLIB
    "${MFD_CLIENT_TEST_PACKAGE_DEV_ROOT}/libs/${MFD_PACKAGE_TOOLSET}/${MFD_EXEC_PLATFORM_DIR}/Release/mfd_client_api.lib")
set(MFD_CLIENT_TEST_PACKAGE_HAS_DEBUG_IMPLIB OFF)
set(MFD_CLIENT_TEST_PACKAGE_HAS_RELEASE_IMPLIB OFF)
if(EXISTS "${MFD_CLIENT_TEST_PACKAGE_DEBUG_IMPLIB}")
    set(MFD_CLIENT_TEST_PACKAGE_HAS_DEBUG_IMPLIB ON)
endif()
if(EXISTS "${MFD_CLIENT_TEST_PACKAGE_RELEASE_IMPLIB}")
    set(MFD_CLIENT_TEST_PACKAGE_HAS_RELEASE_IMPLIB ON)
endif()

set(MFD_CLIENT_TEST_PACKAGE_RUNTIME_TOOLSET "")
foreach(mfd_client_test_package_toolset_candidate
        "${MFD_PACKAGE_TOOLSET}"
        "native")
    if(EXISTS
       "${MFD_CLIENT_TEST_PACKAGE_DELIVERY_ROOT}/packages_bin_windows/MFDStudioClientApi.Install/_Exec/${mfd_client_test_package_toolset_candidate}/${MFD_EXEC_PLATFORM_DIR}/Debug/mfd_client_api.dll"
       OR EXISTS
       "${MFD_CLIENT_TEST_PACKAGE_DELIVERY_ROOT}/packages_bin_windows/MFDStudioClientApi.Install/_Exec/${mfd_client_test_package_toolset_candidate}/${MFD_EXEC_PLATFORM_DIR}/Release/mfd_client_api.dll")
        set(MFD_CLIENT_TEST_PACKAGE_RUNTIME_TOOLSET "${mfd_client_test_package_toolset_candidate}")
        break()
    endif()
endforeach()

set(MFD_CLIENT_TEST_PACKAGE_RUNTIME_ROOT
    "${MFD_CLIENT_TEST_PACKAGE_DELIVERY_ROOT}/packages_bin_windows/MFDStudioClientApi.Install/_Exec/${MFD_CLIENT_TEST_PACKAGE_RUNTIME_TOOLSET}/${MFD_EXEC_PLATFORM_DIR}")
set(MFD_CLIENT_TEST_PACKAGE_DEBUG_DLL
    "${MFD_CLIENT_TEST_PACKAGE_RUNTIME_ROOT}/Debug/mfd_client_api.dll")
set(MFD_CLIENT_TEST_PACKAGE_RELEASE_DLL
    "${MFD_CLIENT_TEST_PACKAGE_RUNTIME_ROOT}/Release/mfd_client_api.dll")
set(MFD_CLIENT_TEST_PACKAGE_HAS_DEBUG_DLL OFF)
set(MFD_CLIENT_TEST_PACKAGE_HAS_RELEASE_DLL OFF)
if(EXISTS "${MFD_CLIENT_TEST_PACKAGE_DEBUG_DLL}")
    set(MFD_CLIENT_TEST_PACKAGE_HAS_DEBUG_DLL ON)
endif()
if(EXISTS "${MFD_CLIENT_TEST_PACKAGE_RELEASE_DLL}")
    set(MFD_CLIENT_TEST_PACKAGE_HAS_RELEASE_DLL ON)
endif()

set(MFD_CLIENT_TEST_PACKAGE_AVAILABLE_CONFIGS "")
if(MFD_CLIENT_TEST_PACKAGE_HAS_DEBUG_IMPLIB AND MFD_CLIENT_TEST_PACKAGE_HAS_DEBUG_DLL)
    list(APPEND MFD_CLIENT_TEST_PACKAGE_AVAILABLE_CONFIGS Debug)
endif()
if(MFD_CLIENT_TEST_PACKAGE_HAS_RELEASE_IMPLIB AND MFD_CLIENT_TEST_PACKAGE_HAS_RELEASE_DLL)
    list(APPEND MFD_CLIENT_TEST_PACKAGE_AVAILABLE_CONFIGS Release)
endif()

if(NOT EXISTS "${MFD_CLIENT_TEST_PACKAGE_CONFIG_FILE}"
   OR MFD_CLIENT_TEST_PACKAGE_RUNTIME_TOOLSET STREQUAL ""
   OR MFD_CLIENT_TEST_PACKAGE_AVAILABLE_CONFIGS STREQUAL "")
    message(STATUS
        "Skipping client_test_package because the MFDStudioClientApi delivery packages are incomplete.\n"
        "Run Scripts\\BuildDeliveries.bat --version <value> to populate _Deliveries/.")
    return()
endif()

string(JOIN ", " MFD_CLIENT_TEST_PACKAGE_AVAILABLE_CONFIGS_TEXT
    ${MFD_CLIENT_TEST_PACKAGE_AVAILABLE_CONFIGS})
message(STATUS
    "Enabling client_test_package with MFDStudioClientApi delivery configs: "
    "${MFD_CLIENT_TEST_PACKAGE_AVAILABLE_CONFIGS_TEXT}")

find_package(MFDStudioClientApi REQUIRED CONFIG
    PATHS "${MFD_CLIENT_TEST_PACKAGE_DEV_ROOT}"
    NO_DEFAULT_PATH)

set(MFD_CLIENT_TEST_PACKAGE_WINDOW_JSON
    "${MFD_ROOT_DIR}/assets/windows/package_test_window.json")
set(MFD_CLIENT_TEST_PACKAGE_GENERATED_DIR
    "${CMAKE_CURRENT_SOURCE_DIR}/generated")
set(MFD_CLIENT_TEST_PACKAGE_GENERATED_HEADER
    "${MFD_CLIENT_TEST_PACKAGE_GENERATED_DIR}/PackageTestUi.h")
set(MFD_CLIENT_TEST_PACKAGE_GENERATED_SOURCE
    "${MFD_CLIENT_TEST_PACKAGE_GENERATED_DIR}/PackageTestUi.cpp")
set(MFD_CLIENT_TEST_PACKAGE_GENERATED_MAP
    "${MFD_ROOT_DIR}/assets/windows/package_test_window.generated.map")
set(MFD_CLIENT_TEST_PACKAGE_RUNTIME_DLL
    "$<$<CONFIG:Debug>:${MFD_CLIENT_TEST_PACKAGE_DEBUG_DLL}>$<$<NOT:$<CONFIG:Debug>>:${MFD_CLIENT_TEST_PACKAGE_RELEASE_DLL}>")

client_api_generate_ui(
    WINDOW_JSON "${MFD_CLIENT_TEST_PACKAGE_WINDOW_JSON}"
    OUTPUT_HEADER "${MFD_CLIENT_TEST_PACKAGE_GENERATED_HEADER}"
    OUTPUT_SOURCE "${MFD_CLIENT_TEST_PACKAGE_GENERATED_SOURCE}"
    OUTPUT_MAP "${MFD_CLIENT_TEST_PACKAGE_GENERATED_MAP}"
    NAMESPACE "package_test_ui"
    UI_CLASS_NAME "PackageTestUi"
    HEADER_INCLUDE "PackageTestUi.h")

set(MFD_CLIENT_TEST_PACKAGE_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp"
    "${MFD_CLIENT_TEST_PACKAGE_GENERATED_SOURCE}")

add_executable(client_test_package
    "${MFD_CLIENT_TEST_PACKAGE_GENERATED_HEADER}"
    ${MFD_CLIENT_TEST_PACKAGE_SOURCES})

target_include_directories(client_test_package
    PRIVATE
        "${MFD_CLIENT_TEST_PACKAGE_GENERATED_DIR}")

target_link_libraries(client_test_package
    PRIVATE
        MFDStudio::ClientApi)

target_compile_features(client_test_package PRIVATE cxx_std_17)

set_target_properties(client_test_package PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:client_test_package>")

if(NOT MFD_CLIENT_TEST_PACKAGE_HAS_DEBUG_IMPLIB OR NOT MFD_CLIENT_TEST_PACKAGE_HAS_DEBUG_DLL)
    set_property(TARGET client_test_package PROPERTY EXCLUDE_FROM_DEFAULT_BUILD_DEBUG ON)
endif()

if(NOT MFD_CLIENT_TEST_PACKAGE_HAS_RELEASE_IMPLIB OR NOT MFD_CLIENT_TEST_PACKAGE_HAS_RELEASE_DLL)
    set_property(TARGET client_test_package PROPERTY EXCLUDE_FROM_DEFAULT_BUILD_RELEASE ON)
    set_property(TARGET client_test_package PROPERTY EXCLUDE_FROM_DEFAULT_BUILD_MINSIZEREL ON)
    set_property(TARGET client_test_package PROPERTY EXCLUDE_FROM_DEFAULT_BUILD_RELWITHDEBINFO ON)
endif()

add_custom_command(
    TARGET client_test_package
    POST_BUILD
    COMMAND
        ${CMAKE_COMMAND}
        "-DSOURCE=${MFD_CLIENT_TEST_PACKAGE_RUNTIME_DLL}"
        "-DDEST_FILE=$<TARGET_FILE_DIR:client_test_package>/mfd_client_api.dll"
        -P
        "${MFD_ROOT_DIR}/cmake/CopyIfExists.cmake"
    VERBATIM)
