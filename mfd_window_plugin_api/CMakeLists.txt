# This file is part of MFDStudio.
# Project author: Benoit Fra
# Repository: https://github.com/benoitfragit/MFDStudio

set(MFD_WINDOW_PLUGIN_API_PUBLIC_HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/include/mfd/window/WindowLauncherPlugin.h)

set(MFD_WINDOW_PLUGIN_API_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/WindowLauncherPlugin.cpp)

add_library(mfd_window_plugin_api SHARED
    ${MFD_WINDOW_PLUGIN_API_PUBLIC_HEADERS}
    ${MFD_WINDOW_PLUGIN_API_SOURCES})

add_library(mfd::window_plugin_api ALIAS mfd_window_plugin_api)

target_include_directories(mfd_window_plugin_api
    PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
        "$<INSTALL_INTERFACE:include>")

target_compile_features(mfd_window_plugin_api PUBLIC cxx_std_17)
target_compile_definitions(mfd_window_plugin_api
    PUBLIC
        MFD_WINDOW_PLUGIN_API_DLL
    PRIVATE
        MFD_WINDOW_PLUGIN_API_EXPORTS)

mfd_enable_target_warnings(mfd_window_plugin_api)

set(MFD_WINDOW_PLUGIN_PACKAGE_CONFIG_FILE
    "${CMAKE_CURRENT_BINARY_DIR}/MFDStudioWindowLauncherPluginConfig.cmake")
set(MFD_WINDOW_PLUGIN_PACKAGE_CONFIG_VERSION_FILE
    "${CMAKE_CURRENT_BINARY_DIR}/MFDStudioWindowLauncherPluginConfigVersion.cmake")

set(MFD_PACKAGE_IMPORTED_TARGET_NAME "MFDStudio::WindowLauncherPlugin")
configure_file(
    "${MFD_ROOT_DIR}/cmake/packages/MFDStudioWindowLauncherPluginConfig.cmake.in"
    "${MFD_WINDOW_PLUGIN_PACKAGE_CONFIG_FILE}"
    @ONLY)
unset(MFD_PACKAGE_IMPORTED_TARGET_NAME)

write_basic_package_version_file(
    "${MFD_WINDOW_PLUGIN_PACKAGE_CONFIG_VERSION_FILE}"
    VERSION "${MFD_PACKAGE_VERSION}"
    COMPATIBILITY SameMajorVersion
	ARCH_INDEPENDENT)

install(TARGETS mfd_window_plugin_api
    ARCHIVE DESTINATION "${MFD_PACKAGE_LIB_DESTINATION}" COMPONENT ${MFD_COMPONENT_WINDOW_PLUGIN_DEVELOPMENT}
    LIBRARY DESTINATION "${MFD_PACKAGE_LIB_DESTINATION}" COMPONENT ${MFD_COMPONENT_WINDOW_PLUGIN_DEVELOPMENT}
    RUNTIME DESTINATION . COMPONENT ${MFD_COMPONENT_WINDOW_LAUNCHER_RUNTIME})

install(FILES
    ${MFD_WINDOW_PLUGIN_API_PUBLIC_HEADERS}
    DESTINATION include/mfd/window
    COMPONENT ${MFD_COMPONENT_WINDOW_PLUGIN_DEVELOPMENT})

install(FILES
    "${MFD_WINDOW_PLUGIN_PACKAGE_CONFIG_FILE}"
    "${MFD_WINDOW_PLUGIN_PACKAGE_CONFIG_VERSION_FILE}"
    DESTINATION cmake
    COMPONENT ${MFD_COMPONENT_WINDOW_PLUGIN_DEVELOPMENT})
