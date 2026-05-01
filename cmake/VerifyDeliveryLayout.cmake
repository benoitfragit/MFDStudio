# This file is part of MFDStudio.
# Project author: Benoit Fra
# Repository: https://github.com/benoitfragit/MFDStudio
foreach(required_var
        BUILD_DIRECTORY
        BUILD_CONFIG
        DELIVERIES_ROOT
        TOOLSET
        PLATFORM)
    if(NOT DEFINED ${required_var} OR "${${required_var}}" STREQUAL "")
        message(FATAL_ERROR "VerifyDeliveryLayout.cmake requires ${required_var}.")
    endif()
endforeach()

execute_process(
    COMMAND
        ${CMAKE_COMMAND}
        --build
        "${BUILD_DIRECTORY}"
        --config
        "${BUILD_CONFIG}"
        --target
        stage_deliveries
    RESULT_VARIABLE stage_result
    OUTPUT_VARIABLE stage_stdout
    ERROR_VARIABLE stage_stderr)

if(NOT stage_result EQUAL 0)
    message(FATAL_ERROR
        "Failed to build stage_deliveries before verifying package layout.\n"
        "stdout:\n${stage_stdout}\n"
        "stderr:\n${stage_stderr}")
endif()

function(expect_exists path_description file_path)
    if(NOT EXISTS "${file_path}")
        message(FATAL_ERROR "${path_description} is missing: ${file_path}")
    endif()
endfunction()

function(expect_missing path_description file_path)
    if(EXISTS "${file_path}")
        message(FATAL_ERROR "${path_description} must not be packaged: ${file_path}")
    endif()
endfunction()

set(dev_root "${DELIVERIES_ROOT}/packages_windows")
set(runtime_root "${DELIVERIES_ROOT}/packages_bin_windows")
set(config_runtime_suffix "_Exec/${TOOLSET}/${PLATFORM}/${BUILD_CONFIG}")

set(client_sdk_root "${dev_root}/MFDStudioClientApi/build/native")
expect_exists("Client API development package include" "${client_sdk_root}/include/mfd/client/Animation.h")
expect_exists("Client API generator-facing command client header" "${client_sdk_root}/include/mfd/control/CommandClient.h")
expect_exists("Client API support ArrayView header" "${client_sdk_root}/include/mfd/core/ArrayView.h")
expect_exists("Client API package config" "${client_sdk_root}/cmake/MFDStudioClientApiConfig.cmake")
expect_exists("Client API generator helper" "${client_sdk_root}/cmake/ClientApiGenerator.cmake")
expect_exists("Client API generator script" "${client_sdk_root}/tools/generator/scripts/generate_ui.py")
expect_exists("Client API low-level import library" "${client_sdk_root}/libs/${TOOLSET}/${PLATFORM}/${BUILD_CONFIG}/mfd_api.lib")
expect_exists("Client API import library" "${client_sdk_root}/libs/${TOOLSET}/${PLATFORM}/${BUILD_CONFIG}/mfd_client_api.lib")

set(editor_sdk_root "${dev_root}/MFDStudioEditorPlugin/build/native")
expect_exists("Editor plugin SDK include" "${editor_sdk_root}/include/mfd/editor/AutomationPlugin.h")
expect_exists("Editor plugin SDK config" "${editor_sdk_root}/cmake/MFDStudioEditorPluginConfig.cmake")
expect_exists("Editor plugin sample" "${editor_sdk_root}/examples/mfd_editor_automation_sample_plugin/CMakeLists.txt")
expect_missing("Editor plugin SDK binary libraries" "${editor_sdk_root}/libs")

set(window_sdk_root "${dev_root}/MFDStudioWindowLauncherPlugin/build/native")
expect_exists("Window launcher plugin SDK include" "${window_sdk_root}/include/mfd/window/WindowLauncherPlugin.h")
expect_exists("Window launcher plugin SDK config" "${window_sdk_root}/cmake/MFDStudioWindowLauncherPluginConfig.cmake")
expect_exists("Window launcher plugin support library" "${window_sdk_root}/libs/${TOOLSET}/${PLATFORM}/${BUILD_CONFIG}/mfd_window_plugin_api.lib")
expect_missing("Window launcher plugin packaged example" "${window_sdk_root}/examples")

set(client_runtime_root "${runtime_root}/MFDStudioClientApi.Install/${config_runtime_suffix}")
expect_exists("Client API install package manifest" "${runtime_root}/MFDStudioClientApi.Install/_Exec/_Conf/package_manifest.json")
expect_exists("Client API install low-level DLL" "${client_runtime_root}/mfd_api.dll")
expect_exists("Client API install DLL" "${client_runtime_root}/mfd_client_api.dll")
expect_missing("Client API install legacy low-level DLL name" "${client_runtime_root}/mfd_public_api.dll")
expect_missing("Client API install assets" "${client_runtime_root}/assets")

set(editor_runtime_root "${runtime_root}/MFDStudioEditor.Install/${config_runtime_suffix}")
expect_exists("Editor install package manifest" "${runtime_root}/MFDStudioEditor.Install/_Exec/_Conf/package_manifest.json")
expect_exists("Editor install executable" "${editor_runtime_root}/mfd_editor.exe")
expect_exists("Editor install branding icon" "${editor_runtime_root}/branding/mfdstudio_app_icon.png")
expect_missing("Editor install demo assets" "${editor_runtime_root}/assets")
expect_missing("Editor install demo launcher script" "${editor_runtime_root}/Start-MfdDemo.bat")

set(window_runtime_root "${runtime_root}/MFDStudioWindowLauncher.Install/${config_runtime_suffix}")
expect_exists("Window launcher install package manifest" "${runtime_root}/MFDStudioWindowLauncher.Install/_Exec/_Conf/package_manifest.json")
expect_exists("Window launcher install executable" "${window_runtime_root}/mfd_window.exe")
expect_exists("Window launcher install plugin API DLL" "${window_runtime_root}/mfd_window_plugin_api.dll")
expect_exists("Window launcher install launcher script" "${window_runtime_root}/Start-MfdWindow.bat")
expect_exists("Window launcher install branding icon" "${window_runtime_root}/branding/mfdstudio_app_icon.png")
expect_missing("Window launcher install demo assets" "${window_runtime_root}/assets")
expect_missing("Window launcher install sample framebuffer plugin" "${window_runtime_root}/mfd_framebuffer_stdout_plugin.dll")
expect_missing("Window launcher install demo launcher" "${window_runtime_root}/Start-MfdDemo.bat")
expect_missing("Window launcher install cockpit launcher" "${window_runtime_root}/Start-MfdCockpit.bat")
expect_missing("Window launcher install minimal launcher" "${window_runtime_root}/Start-MfdMinimal.bat")
expect_missing("Window launcher install tutorial launcher" "${window_runtime_root}/Start-MfdTutorial.bat")
