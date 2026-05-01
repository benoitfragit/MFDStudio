# This file is part of MFDStudio.
# Project author: Benoit Fra
# Repository: https://github.com/benoitfragit/MFDStudio

set(MFD_EDITOR_AUTOMATION_PLUGIN_LINK_TARGET "")
if(TARGET mfd::editor_plugin_api)
    set(MFD_EDITOR_AUTOMATION_PLUGIN_LINK_TARGET mfd::editor_plugin_api)
elseif(TARGET MFDStudio::EditorPluginApi)
    set(MFD_EDITOR_AUTOMATION_PLUGIN_LINK_TARGET MFDStudio::EditorPluginApi)
else()
    find_package(MFDStudioEditorPlugin REQUIRED CONFIG)
    set(MFD_EDITOR_AUTOMATION_PLUGIN_LINK_TARGET MFDStudio::EditorPluginApi)
endif()

add_library(mfd_editor_automation_sample_plugin SHARED
    ${CMAKE_CURRENT_SOURCE_DIR}/src/EditorAutomationSamplePlugin.cpp)

target_link_libraries(mfd_editor_automation_sample_plugin
    PRIVATE
        ${MFD_EDITOR_AUTOMATION_PLUGIN_LINK_TARGET})

target_compile_features(mfd_editor_automation_sample_plugin PRIVATE cxx_std_17)
target_compile_definitions(mfd_editor_automation_sample_plugin PRIVATE MFD_EDITOR_AUTOMATION_PLUGIN_EXPORTS=1)

mfd_enable_target_warnings(mfd_editor_automation_sample_plugin)

set_target_properties(mfd_editor_automation_sample_plugin PROPERTIES
    OUTPUT_NAME "mfd_editor_automation_sample_plugin"
    VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:mfd_editor_automation_sample_plugin>")

mfd_stage_runtime(mfd_editor_automation_sample_plugin)
