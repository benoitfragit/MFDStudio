@echo off
setlocal EnableExtensions EnableDelayedExpansion

set "SCRIPT_DIR=%~dp0"
set "LAUNCH_ROOT=%SCRIPT_DIR%"
if not exist "%LAUNCH_ROOT%_Exec" (
    if exist "%SCRIPT_DIR%..\_Exec" (
        for %%I in ("%SCRIPT_DIR%..") do set "LAUNCH_ROOT=%%~fI\"
    )
)

set "CLIENT_DIR="
set "CLIENT_EXE="
set "PASSTHROUGH_ARGS="
set "WAIT_FOR_APP=0"
set "HELP_REQUESTED=0"

:parse_args
if "%~1"=="" goto parsed_args
if /I "%~1"=="--client-dir" (
    if "%~2"=="" (
        echo Missing path after --client-dir.
        exit /b 1
    )
    set "CLIENT_DIR=%~2"
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
if "%HELP_REQUESTED%"=="1" goto print_usage

call :resolve_client_dir
if errorlevel 1 exit /b 1

if "%WAIT_FOR_APP%"=="1" goto launch_sync

start "" /D "%CLIENT_DIR%" "%CLIENT_EXE%" %PASSTHROUGH_ARGS%
set "EXIT_CODE=%ERRORLEVEL%"
exit /b %EXIT_CODE%

:print_usage
echo Usage: Start-LHLD.bat [--client-dir ^<dir^>] [--wait] [extra client args]
exit /b 0

:launch_sync
pushd "%CLIENT_DIR%" >nul
call "%CLIENT_EXE%" %PASSTHROUGH_ARGS%
set "EXIT_CODE=%ERRORLEVEL%"
popd >nul
exit /b %EXIT_CODE%

:append_arg
set "%~1=!%~1! "%~2""
exit /b 0

:resolve_client_dir
if defined CLIENT_DIR (
    if not exist "%CLIENT_DIR%\lhld_client.exe" (
        echo Unable to find lhld_client.exe in "%CLIENT_DIR%".
        exit /b 1
    )
    set "CLIENT_EXE=%CLIENT_DIR%\lhld_client.exe"
    exit /b 0
)

if exist "%SCRIPT_DIR%lhld_client.exe" (
    set "CLIENT_DIR=%SCRIPT_DIR%"
    set "CLIENT_EXE=%SCRIPT_DIR%lhld_client.exe"
    exit /b 0
)

set "STAGED_ROOT=%LAUNCH_ROOT%_Exec"
if not exist "%STAGED_ROOT%" (
    echo Unable to locate "_Exec" from "%LAUNCH_ROOT%". Build lhld_client first or pass --client-dir.
    exit /b 1
)

for /d %%T in ("%STAGED_ROOT%\*") do (
    for %%R in ("win32\Debug" "win32\RelWithDebInfo" "win32\Release" "x64\Debug" "x64\RelWithDebInfo" "x64\Release") do (
        if exist "%%~fT\%%~R\lhld_client.exe" (
            set "CLIENT_DIR=%%~fT\%%~R"
            set "CLIENT_EXE=%%~fT\%%~R\lhld_client.exe"
            exit /b 0
        )
    )
)

for /f "delims=" %%I in ('dir /b /s "%STAGED_ROOT%\lhld_client.exe" 2^>nul') do (
    set "CLIENT_DIR=%%~dpI"
    set "CLIENT_EXE=%%~fI"
    exit /b 0
)

echo Unable to find lhld_client.exe under "%STAGED_ROOT%". Build lhld_client first.
exit /b 1
