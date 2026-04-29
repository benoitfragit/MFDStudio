@echo off
setlocal
set "MFD_DEFAULT_FRAMEBUFFER_PLUGIN=mfd_framebuffer_stdout_plugin.dll"

set "SCRIPT_DIR=%~dp0"
set "LAUNCH_ROOT=%SCRIPT_DIR%"
if not exist "%LAUNCH_ROOT%assets" (
    if exist "%SCRIPT_DIR%..\assets" (
        for %%I in ("%SCRIPT_DIR%..") do set "LAUNCH_ROOT=%%~fI\"
    )
)

if not exist "%LAUNCH_ROOT%assets\windows\mfd_tutorial.json" if not exist "%CD%\assets\windows\mfd_tutorial.json" (
    echo Tutorial window asset set is incomplete.
    echo Finish the integrated editor tutorial and save "assets\windows\mfd_tutorial.json" before using Start-MfdTutorial.bat.
    exit /b 1
)

call "%~dp0Start-MfdWindow.bat" "assets/windows/mfd_tutorial.json" %*
exit /b %ERRORLEVEL%
