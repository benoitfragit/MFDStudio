# This file is part of MFDStudio.
# Project author: Benoit Fra
# Repository: https://github.com/benoitfragit/MFDStudio
set(CLIENT_API_PUBLIC_HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/include/mfd/client/Animation.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/mfd/client/ClientSdk.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/mfd/client/ClientExport.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/mfd/client/GeneratedUiSupport.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/mfd/client/LatestBatchPublisher.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/mfd/client/WindowLivenessMonitor.h)

set(CLIENT_API_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Animation.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/LatestBatchPublisher.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/WindowLivenessMonitor.cpp)

add_library(mfd_client_api SHARED
    ${CLIENT_API_PUBLIC_HEADERS}
    ${CLIENT_API_SOURCES}
    $<TARGET_OBJECTS:mfd_model_objects>
    $<TARGET_OBJECTS:mfd_transport_objects>
    $<TARGET_OBJECTS:mfd_runtime_objects>
    $<TARGET_OBJECTS:mfd_io_json_objects>)

add_library(mfd::client ALIAS mfd_client_api)

target_include_directories(mfd_client_api
    PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
        "$<BUILD_INTERFACE:${MFD_COMMON_API_PUBLIC_INCLUDE_DIR}>"
        "$<BUILD_INTERFACE:${MFD_API_PUBLIC_INCLUDE_DIR}>"
        "$<INSTALL_INTERFACE:include>")

# `mfd_client_api` statically bundles the shared `mfd_*_objects` (compiled with
# MFD_API_EXPORTS, i.e. __declspec(dllexport)) instead of linking mfd_api.dll, so
# its own sources must see the common API as exported-and-local, not imported.
# Consumers, in contrast, import every re-exported symbol from this single DLL.
# The *_DLL (dllimport) decorations are therefore INTERFACE-only; the build of
# this target itself exports both surfaces explicitly (no WINDOWS_EXPORT_ALL_SYMBOLS).
target_compile_definitions(mfd_client_api
    INTERFACE
        MFD_API_DLL
        MFD_CLIENT_API_DLL
    PRIVATE
        MFD_API_EXPORTS
        MFD_CLIENT_API_EXPORTS)

target_compile_features(mfd_client_api PUBLIC cxx_std_17)
target_link_libraries(mfd_client_api
    PRIVATE
        EnTT::EnTT
        nlohmann_json::nlohmann_json
        protobuf::libprotobuf)

if(WIN32)
    target_link_libraries(mfd_client_api PRIVATE ws2_32)
endif()

mfd_enable_target_warnings(mfd_client_api)

mfd_stage_runtime(mfd_client_api WITH_RUNTIME_DLLS)

set(MFD_CLIENT_API_PACKAGE_CONFIG_FILE
    "${CMAKE_CURRENT_BINARY_DIR}/MFDStudioClientApiConfig.cmake")
set(MFD_CLIENT_API_PACKAGE_CONFIG_VERSION_FILE
    "${CMAKE_CURRENT_BINARY_DIR}/MFDStudioClientApiConfigVersion.cmake")

set(MFD_PACKAGE_IMPORTED_TARGET_NAME "MFDStudio::ClientApi")
configure_file(
    "${MFD_ROOT_DIR}/cmake/packages/MFDStudioClientApiConfig.cmake.in"
    "${MFD_CLIENT_API_PACKAGE_CONFIG_FILE}"
    @ONLY)
unset(MFD_PACKAGE_IMPORTED_TARGET_NAME)

write_basic_package_version_file(
    "${MFD_CLIENT_API_PACKAGE_CONFIG_VERSION_FILE}"
    VERSION "${MFD_PACKAGE_VERSION}"
    COMPATIBILITY SameMajorVersion
	ARCH_INDEPENDENT)

install(TARGETS mfd_client_api
    ARCHIVE DESTINATION "${MFD_PACKAGE_LIB_DESTINATION}" COMPONENT ${MFD_COMPONENT_CLIENT_API_DEVELOPMENT}
    LIBRARY DESTINATION "${MFD_PACKAGE_LIB_DESTINATION}" COMPONENT ${MFD_COMPONENT_CLIENT_API_DEVELOPMENT}
    RUNTIME DESTINATION . COMPONENT ${MFD_COMPONENT_CLIENT_API_RUNTIME})

install(FILES
    ${CLIENT_API_PUBLIC_HEADERS}
    DESTINATION include/mfd/client
    COMPONENT ${MFD_COMPONENT_CLIENT_API_DEVELOPMENT})
install(FILES
    ${MFD_ROOT_DIR}/mfd_common_api/include/mfd/MfdExport.h
    DESTINATION include/mfd
    COMPONENT ${MFD_COMPONENT_CLIENT_API_DEVELOPMENT})
install(FILES
    ${MFD_ROOT_DIR}/mfd_common_api/include/mfd/core/ArrayView.h
    DESTINATION include/mfd/core
    COMPONENT ${MFD_COMPONENT_CLIENT_API_DEVELOPMENT})
install(FILES
    ${MFD_ROOT_DIR}/mfd_common_api/include/mfd/control/CommandClient.h
    ${MFD_ROOT_DIR}/mfd_common_api/include/mfd/control/CommandTransport.h
    ${MFD_ROOT_DIR}/mfd_common_api/include/mfd/control/CommandTypes.h
    ${MFD_ROOT_DIR}/mfd_common_api/include/mfd/control/FeedbackTransport.h
    ${MFD_ROOT_DIR}/mfd_common_api/include/mfd/control/StrobeFeedback.h
    ${MFD_ROOT_DIR}/mfd_api/include/mfd/control/UserSpaceProjector.h
    DESTINATION include/mfd/control
    COMPONENT ${MFD_COMPONENT_CLIENT_API_DEVELOPMENT})
install(FILES
    ${MFD_ROOT_DIR}/mfd_common_api/include/mfd/ipc/ExchangeChannel.h
    ${MFD_ROOT_DIR}/mfd_common_api/include/mfd/ipc/UdpLimits.h
    DESTINATION include/mfd/ipc
    COMPONENT ${MFD_COMPONENT_CLIENT_API_DEVELOPMENT})
install(FILES
    ${MFD_ROOT_DIR}/mfd_common_api/include/mfd/model/PageDefinition.h
    ${MFD_ROOT_DIR}/mfd_common_api/include/mfd/model/PageName.h
    ${MFD_ROOT_DIR}/mfd_common_api/include/mfd/model/Reticle.h
    ${MFD_ROOT_DIR}/mfd_common_api/include/mfd/model/Types.h
    DESTINATION include/mfd/model
    COMPONENT ${MFD_COMPONENT_CLIENT_API_DEVELOPMENT})
install(FILES
    ${MFD_ROOT_DIR}/mfd_common_api/include/mfd/runtime/GeneratedTransportMap.h
    DESTINATION include/mfd/runtime
    COMPONENT ${MFD_COMPONENT_CLIENT_API_DEVELOPMENT})
install(FILES
    ${MFD_ROOT_DIR}/mfd_api/include/mfd/io/JsonLoader.h
    DESTINATION include/mfd/io
    COMPONENT ${MFD_COMPONENT_CLIENT_API_DEVELOPMENT})

install(FILES
    "${MFD_CLIENT_API_PACKAGE_CONFIG_FILE}"
    "${MFD_CLIENT_API_PACKAGE_CONFIG_VERSION_FILE}"
    "${CMAKE_CURRENT_SOURCE_DIR}/generator/ClientApiGenerator.cmake"
    DESTINATION cmake
    COMPONENT ${MFD_COMPONENT_CLIENT_API_DEVELOPMENT})

install(DIRECTORY
    "${CMAKE_CURRENT_SOURCE_DIR}/generator/scripts"
    DESTINATION tools/generator
    COMPONENT ${MFD_COMPONENT_CLIENT_API_DEVELOPMENT}
    FILES_MATCHING
        PATTERN "*.py")
