# This file is part of MFDStudio.
# Project author: Benoit Fra
# Repository: https://github.com/benoitfragit/MFDStudio
foreach(required_var
        BUILD_DIRECTORY
        BUILD_CONFIG
        EXEC_ROOT
        TOOLSET
        PLATFORM)
    if(NOT DEFINED ${required_var} OR "${${required_var}}" STREQUAL "")
        message(FATAL_ERROR "VerifyExecLayout.cmake requires ${required_var}.")
    endif()
endforeach()

set(config_root "${EXEC_ROOT}/${TOOLSET}/${PLATFORM}/${BUILD_CONFIG}")
set(exec_stage_sentinel "${config_root}/mfd_window.exe")

if(NOT EXISTS "${exec_stage_sentinel}")
    execute_process(
        COMMAND
            ${CMAKE_COMMAND}
            --build
            "${BUILD_DIRECTORY}"
            --config
            "${BUILD_CONFIG}"
            --target
            stage_exec
        RESULT_VARIABLE stage_result
        OUTPUT_VARIABLE stage_stdout
        ERROR_VARIABLE stage_stderr)

    if(NOT stage_result EQUAL 0)
        message(FATAL_ERROR
            "Failed to build stage_exec before verifying the staged runtime layout.\n"
            "stdout:\n${stage_stdout}\n"
            "stderr:\n${stage_stderr}")
    endif()
endif()

function(expect_exists path_description file_path)
    if(NOT EXISTS "${file_path}")
        message(FATAL_ERROR "${path_description} is missing: ${file_path}")
    endif()
endfunction()

function(expect_missing path_description file_path)
    if(EXISTS "${file_path}")
        message(FATAL_ERROR "${path_description} must not be staged: ${file_path}")
    endif()
endfunction()

expect_exists("Exec root staged window launcher" "${config_root}/mfd_window.exe")
expect_exists("Exec root staged editor" "${config_root}/mfd_editor.exe")
expect_exists("Exec root staged client API DLL" "${config_root}/mfd_client_api.dll")
expect_exists("Exec root staged low-level API DLL" "${config_root}/mfd_api.dll")
expect_exists("Exec root staged launch script" "${config_root}/Start-MfdWindow.bat")
expect_exists("Exec root staged demo launch script" "${config_root}/Start-MfdDemo.bat")
expect_exists("Exec root staged HUD launch script" "${config_root}/Start-DemoHUD.bat")
expect_exists("Exec root staged radar load launch script" "${config_root}/Start-RadarLoad.bat")
expect_exists("Exec root staged package test launch script" "${config_root}/Start-MfdPackageTest.bat")
expect_exists("Exec root staged tutorial launch script" "${config_root}/Start-Tutorial.bat")
expect_exists("Exec root staged branding icon" "${config_root}/branding/mfdstudio_app_icon.png")
expect_exists("Exec root staged demo asset" "${config_root}/assets/windows/demo_window.json")
expect_exists("Exec root staged HUD window asset" "${config_root}/assets/windows/demo_hud_window.json")
expect_exists("Exec root staged radar load window asset" "${config_root}/assets/windows/radar_load_window.json")
expect_exists("Exec root staged package test window asset" "${config_root}/assets/windows/package_test_window.json")
expect_exists("Exec root staged demo primitives page" "${config_root}/assets/pages/primitives.json")
expect_exists("Exec root staged demo blink page" "${config_root}/assets/pages/blink_strobe.json")
expect_exists("Exec root staged demo clipping page" "${config_root}/assets/pages/clipping.json")
expect_exists("Exec root staged HUD page" "${config_root}/assets/pages/demo_hud.json")
expect_exists("Exec root staged radar load page" "${config_root}/assets/pages/radar_load.json")
expect_exists("Exec root staged package test page" "${config_root}/assets/pages/package_test_page.json")
expect_exists("Exec root staged HUD reticle asset" "${config_root}/assets/reticles/demo_hud_fpm.json")
expect_exists("Exec root staged radar track reticle asset" "${config_root}/assets/reticles/radar_track.json")
expect_exists("Exec root staged package test reticle asset" "${config_root}/assets/reticles/package_test_circle.json")
expect_exists("Exec root staged client API tests" "${config_root}/tests/client_api_tests.exe")
expect_missing("Exec root legacy public API DLL" "${config_root}/mfd_public_api.dll")
expect_missing("Exec root legacy launch script" "${config_root}/Start-MfdWindow.ps1")
