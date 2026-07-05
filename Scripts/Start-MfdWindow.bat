@echo off
setlocal EnableExtensions EnableDelayedExpansion

if "%~1"=="" (
    echo Usage: Start-MfdWindow.bat ^<window.json^> [--runtime-dir ^<dir^>] [--framebuffer-plugin ^<plugin.dll^>] [--wait] [extra launcher args]
    exit /b 1
)

set "SCRIPT_DIR=%~dp0"
set "LAUNCH_ROOT=%SCRIPT_DIR%"
if not exist "%LAUNCH_ROOT%assets" (
    if exist "%SCRIPT_DIR%..\assets" (
        for %%I in ("%SCRIPT_DIR%..") do set "LAUNCH_ROOT=%%~fI\"
    ) else if exist "%SCRIPT_DIR%..\_Exec" (
        for %%I in ("%SCRIPT_DIR%..") do set "LAUNCH_ROOT=%%~fI\"
    )
)

set "WINDOW_FILE=%~1"
shift

set "PASSTHROUGH_ARGS="
set "RUNTIME_DIR="
set "RUNTIME_EXE="
set "EXPLICIT_PLUGIN="
set "WAIT_FOR_APP=0"
set "HELP_REQUESTED=0"

:parse_args
if "%~1"=="" goto parsed_args
if /I "%~1"=="--runtime-dir" (
    if "%~2"=="" (
        echo Missing path after --runtime-dir.
        exit /b 1
    )
    set "RUNTIME_DIR=%~2"
    shift
    shift
    goto parse_args
)
if /I "%~1"=="--framebuffer-plugin" (
    if "%~2"=="" (
        echo Missing path after --framebuffer-plugin.
        exit /b 1
    )
    set "EXPLICIT_PLUGIN=%~2"
    shift
    shift
    goto parse_args
)
if /I "%~1"=="--wait" (
    set "WAIT_FOR_APP=1"
    shift
    goto parse_args
)
if /I "%~1"=="--help" set "HELP_REQUESTED=1"
if /I "%~1"=="-h" set "HELP_REQUESTED=1"
call :append_arg PASSTHROUGH_ARGS "%~1"
shift
goto parse_args

:parsed_args
call :resolve_runtime_dir
if errorlevel 1 exit /b 1

call :resolve_launch_path RESOLVED_WINDOW_FILE "%WINDOW_FILE%"
if errorlevel 1 exit /b 1

set "PLUGIN_FILE=%EXPLICIT_PLUGIN%"
if not defined PLUGIN_FILE if defined MFD_DEFAULT_FRAMEBUFFER_PLUGIN set "PLUGIN_FILE=%MFD_DEFAULT_FRAMEBUFFER_PLUGIN%"
if defined PLUGIN_FILE (
    call :resolve_launch_path RESOLVED_PLUGIN_FILE "%PLUGIN_FILE%"
    if errorlevel 1 exit /b 1
)

set "COMMAND_ARGS="
call :append_arg COMMAND_ARGS "--window"
call :append_arg COMMAND_ARGS "%RESOLVED_WINDOW_FILE%"
if defined PLUGIN_FILE (
    call :append_arg COMMAND_ARGS "--framebuffer-plugin"
    call :append_arg COMMAND_ARGS "%RESOLVED_PLUGIN_FILE%"
)
if defined PASSTHROUGH_ARGS set "COMMAND_ARGS=%COMMAND_ARGS% %PASSTHROUGH_ARGS%"

if "%WAIT_FOR_APP%"=="1" goto launch_sync
if "%HELP_REQUESTED%"=="1" goto launch_sync

start "" /D "%RUNTIME_DIR%" "%RUNTIME_EXE%" %COMMAND_ARGS%
set "EXIT_CODE=%ERRORLEVEL%"
exit /b %EXIT_CODE%

:launch_sync
pushd "%RUNTIME_DIR%" >nul
call "%RUNTIME_EXE%" %COMMAND_ARGS%
set "EXIT_CODE=%ERRORLEVEL%"
popd >nul
exit /b %EXIT_CODE%

:append_arg
set "%~1=!%~1! "%~2""
exit /b 0

:resolve_runtime_dir
if defined RUNTIME_DIR (
    if not exist "%RUNTIME_DIR%\mfd_window.exe" (
        echo Unable to find mfd_window.exe in "%RUNTIME_DIR%".
        exit /b 1
    )
    set "RUNTIME_EXE=%RUNTIME_DIR%\mfd_window.exe"
    exit /b 0
)

if exist "%SCRIPT_DIR%mfd_window.exe" (
    set "RUNTIME_DIR=%SCRIPT_DIR%"
    set "RUNTIME_EXE=%SCRIPT_DIR%mfd_window.exe"
    exit /b 0
)

set "STAGED_ROOT=%LAUNCH_ROOT%_Exec"
if not exist "%STAGED_ROOT%" (
    echo Unable to locate "_Exec" from "%LAUNCH_ROOT%". Build the project first or pass --runtime-dir.
    exit /b 1
)

for /d %%T in ("%STAGED_ROOT%\*") do (
    for %%R in ("win32\Debug" "win32\RelWithDebInfo" "win32\Release" "x64\Debug" "x64\RelWithDebInfo" "x64\Release") do (
        if exist "%%~fT\%%~R\mfd_window.exe" (
            set "RUNTIME_DIR=%%~fT\%%~R"
            set "RUNTIME_EXE=%%~fT\%%~R\mfd_window.exe"
            exit /b 0
        )
    )
)

for /f "delims=" %%I in ('dir /b /s "%STAGED_ROOT%\mfd_window.exe" 2^>nul') do (
    set "RUNTIME_DIR=%%~dpI"
    set "RUNTIME_EXE=%%~fI"
    exit /b 0
)

echo Unable to find mfd_window.exe under "%STAGED_ROOT%". Build mfd_window first.
exit /b 1

:resolve_launch_path
if "%~2"=="" (
    set "%~1="
    exit /b 0
)

if exist "%~f2" (
    set "%~1=%~f2"
    exit /b 0
)

if exist "%RUNTIME_DIR%\%~2" (
    for %%I in ("%RUNTIME_DIR%\%~2") do set "%~1=%%~fI"
    exit /b 0
)

if exist "%LAUNCH_ROOT%%~2" (
    for %%I in ("%LAUNCH_ROOT%%~2") do set "%~1=%%~fI"
    exit /b 0
)

if exist "%SCRIPT_DIR%%~2" (
    for %%I in ("%SCRIPT_DIR%%~2") do set "%~1=%%~fI"
    exit /b 0
)

if exist "%CD%\%~2" (
    for %%I in ("%CD%\%~2") do set "%~1=%%~fI"
    exit /b 0
)

set "%~1=%~2"
exit /b 0
