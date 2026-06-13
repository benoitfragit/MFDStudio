# This file is part of MFDStudio.
# Project author: Benoit Fra
# Repository: https://github.com/benoitfragit/MFDStudio
set(MFD_RUNTIME_API_PUBLIC_HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/include/mfd/runtime_api/OffscreenSurface.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/mfd/runtime_api/RuntimeApiExport.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/mfd/runtime_api/RuntimeSession.h)

set(MFD_RUNTIME_API_PRIVATE_HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/internal/OffscreenPageRenderer.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/internal/RuntimeSessionInternalAccess.hpp)

set(MFD_RUNTIME_API_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/internal/OffscreenPageRenderer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/OffscreenSurface.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/RuntimeSession.cpp)

add_library(mfd_runtime_api SHARED
    ${MFD_RUNTIME_API_PUBLIC_HEADERS}
    ${MFD_RUNTIME_API_PRIVATE_HEADERS}
    ${MFD_RUNTIME_API_SOURCES}
    $<TARGET_OBJECTS:mfd_model_objects>
    $<TARGET_OBJECTS:mfd_transport_objects>
    $<TARGET_OBJECTS:mfd_runtime_objects>
    $<TARGET_OBJECTS:mfd_io_json_objects>)

add_library(mfd::runtime_api ALIAS mfd_runtime_api)

mfd_configure_component(
    mfd_runtime_api
    PUBLIC_INCLUDE_DIRS
        "${CMAKE_CURRENT_SOURCE_DIR}/include")

target_include_directories(mfd_runtime_api
    PRIVATE
        "${MFD_COMMON_API_PUBLIC_INCLUDE_DIR}"
        "${MFD_API_PUBLIC_INCLUDE_DIR}"
        "${MFD_ROOT_DIR}/mfd_api/src"
        "${CMAKE_CURRENT_SOURCE_DIR}/src")

target_compile_definitions(mfd_runtime_api
    PUBLIC
        MFD_RUNTIME_API_DLL
    PRIVATE
        MFD_RUNTIME_API_EXPORTS
        MFD_API_EXPORTS)

target_compile_features(mfd_runtime_api PUBLIC cxx_std_17)
target_link_libraries(mfd_runtime_api
    PRIVATE
        EnTT::EnTT
        mfd::render_raylib
        nlohmann_json::nlohmann_json
        protobuf::libprotobuf)

if(WIN32)
    target_link_libraries(mfd_runtime_api PRIVATE ws2_32)
endif()

mfd_stage_runtime(mfd_runtime_api WITH_RUNTIME_DLLS)

set(MFD_RUNTIME_API_PACKAGE_CONFIG_FILE
    "${CMAKE_CURRENT_BINARY_DIR}/MFDStudioRuntimeApiConfig.cmake")
set(MFD_RUNTIME_API_PACKAGE_CONFIG_VERSION_FILE
    "${CMAKE_CURRENT_BINARY_DIR}/MFDStudioRuntimeApiConfigVersion.cmake")

set(MFD_PACKAGE_IMPORTED_TARGET_NAME "MFDStudio::RuntimeApi")
configure_file(
    "${MFD_ROOT_DIR}/cmake/packages/MFDStudioRuntimeApiConfig.cmake.in"
    "${MFD_RUNTIME_API_PACKAGE_CONFIG_FILE}"
    @ONLY)
unset(MFD_PACKAGE_IMPORTED_TARGET_NAME)

write_basic_package_version_file(
    "${MFD_RUNTIME_API_PACKAGE_CONFIG_VERSION_FILE}"
    VERSION "${MFD_PACKAGE_VERSION}"
    COMPATIBILITY SameMajorVersion
    ARCH_INDEPENDENT)

install(TARGETS mfd_runtime_api
    ARCHIVE DESTINATION "${MFD_PACKAGE_LIB_DESTINATION}" COMPONENT ${MFD_COMPONENT_RUNTIME_API_DEVELOPMENT}
    LIBRARY DESTINATION "${MFD_PACKAGE_LIB_DESTINATION}" COMPONENT ${MFD_COMPONENT_RUNTIME_API_DEVELOPMENT}
    RUNTIME DESTINATION . COMPONENT ${MFD_COMPONENT_RUNTIME_API_RUNTIME})

install(FILES
    ${MFD_RUNTIME_API_PUBLIC_HEADERS}
    DESTINATION include/mfd/runtime_api
    COMPONENT ${MFD_COMPONENT_RUNTIME_API_DEVELOPMENT})

install(FILES
    "${MFD_RUNTIME_API_PACKAGE_CONFIG_FILE}"
    "${MFD_RUNTIME_API_PACKAGE_CONFIG_VERSION_FILE}"
    DESTINATION cmake
    COMPONENT ${MFD_COMPONENT_RUNTIME_API_DEVELOPMENT})
