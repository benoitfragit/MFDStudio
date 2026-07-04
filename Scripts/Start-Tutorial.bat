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

set "WINDOW_FILE=%SCRIPT_DIR%..\examples\tutorial\assets\windows\mfd_tutorial.json"
if not exist "%WINDOW_FILE%" set "WINDOW_FILE=assets\windows\mfd_tutorial.json"

if not exist "%WINDOW_FILE%" if not exist "%LAUNCH_ROOT%assets\windows\mfd_tutorial.json" if not exist "%CD%\examples\tutorial\assets\windows\mfd_tutorial.json" (
    echo Tutorial window asset set is incomplete.
    echo Finish the integrated editor tutorial and save "examples\tutorial\assets\windows\mfd_tutorial.json" before using Start-Tutorial.bat.
    exit /b 1
)

call "%~dp0Start-MfdWindow.bat" "%WINDOW_FILE%" %*
exit /b %ERRORLEVEL%
