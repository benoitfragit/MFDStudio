# This file is part of MFDStudio.
# Project author: Benoit Fra
# Repository: https://github.com/benoitfragit/MFDStudio

add_library(mfd_editor_automation_sample_plugin SHARED
    ${CMAKE_CURRENT_SOURCE_DIR}/src/EditorAutomationSamplePlugin.cpp)

target_link_libraries(mfd_editor_automation_sample_plugin
    PRIVATE
        mfd::editor_plugin_api)

target_compile_features(mfd_editor_automation_sample_plugin PRIVATE cxx_std_17)
target_compile_definitions(mfd_editor_automation_sample_plugin PRIVATE MFD_EDITOR_AUTOMATION_PLUGIN_EXPORTS=1)

mfd_enable_target_warnings(mfd_editor_automation_sample_plugin)

set_target_properties(mfd_editor_automation_sample_plugin PROPERTIES
    OUTPUT_NAME "mfd_editor_automation_sample_plugin"
    VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:mfd_editor_automation_sample_plugin>")

mfd_stage_runtime(mfd_editor_automation_sample_plugin)
