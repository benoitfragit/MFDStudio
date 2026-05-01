# This file is part of MFDStudio.
# Project author: Benoit Fra
# Repository: https://github.com/benoitfragit/MFDStudio
set(MFD_FRAMEBUFFER_STDOUT_PLUGIN_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/FramebufferStdoutPlugin.cpp)

set(MFD_FRAMEBUFFER_STDOUT_PLUGIN_LINK_TARGET "")
if(TARGET mfd::window_plugin_api)
    set(MFD_FRAMEBUFFER_STDOUT_PLUGIN_LINK_TARGET mfd::window_plugin_api)
elseif(TARGET MFDStudio::WindowLauncherPlugin)
    set(MFD_FRAMEBUFFER_STDOUT_PLUGIN_LINK_TARGET MFDStudio::WindowLauncherPlugin)
else()
    find_package(MFDStudioWindowLauncherPlugin REQUIRED CONFIG)
    set(MFD_FRAMEBUFFER_STDOUT_PLUGIN_LINK_TARGET MFDStudio::WindowLauncherPlugin)
endif()

add_library(mfd_framebuffer_stdout_plugin SHARED
    ${MFD_FRAMEBUFFER_STDOUT_PLUGIN_SOURCES})

target_link_libraries(mfd_framebuffer_stdout_plugin
    PRIVATE
        ${MFD_FRAMEBUFFER_STDOUT_PLUGIN_LINK_TARGET})

target_compile_features(mfd_framebuffer_stdout_plugin PRIVATE cxx_std_17)

set_target_properties(mfd_framebuffer_stdout_plugin PROPERTIES
    OUTPUT_NAME "mfd_framebuffer_stdout_plugin"
    VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:mfd_framebuffer_stdout_plugin>")

mfd_stage_runtime(mfd_framebuffer_stdout_plugin)
