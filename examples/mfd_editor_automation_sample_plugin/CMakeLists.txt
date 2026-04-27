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

if(MFD_ENABLE_WARNINGS)
    if(MSVC)
        target_compile_options(mfd_editor_automation_sample_plugin
            PRIVATE
                /W4
                /permissive-
                /Zc:__cplusplus
                /EHsc
                /wd4251
                /wd4275)
    else()
        target_compile_options(mfd_editor_automation_sample_plugin PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endif()

set_target_properties(mfd_editor_automation_sample_plugin PROPERTIES
    OUTPUT_NAME "mfd_editor_automation_sample_plugin"
    VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:mfd_editor_automation_sample_plugin>")

mfd_stage_runtime(mfd_editor_automation_sample_plugin)
