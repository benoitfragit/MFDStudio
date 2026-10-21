@echo off
setlocal EnableExtensions

set "SCRIPT_DIR=%~dp0"
set "WINDOW_FILE=%SCRIPT_DIR%..\examples\apilevel1\assets\windows\containers_window.json"
if not exist "%WINDOW_FILE%" set "WINDOW_FILE=assets\windows\containers_window.json"

set "CLIENT_EXE=%SCRIPT_DIR%apilevel1_containers_client.exe"
if exist "%CLIENT_EXE%" goto launch_example

set "STAGED_ROOT=%SCRIPT_DIR%..\_Exec"
for /d %%T in ("%STAGED_ROOT%\*") do (
    for %%R in ("win32\Debug" "win32\RelWithDebInfo" "win32\Release" "x64\Debug" "x64\RelWithDebInfo" "x64\Release") do (
        if exist "%%~fT\%%~R\apilevel1_containers_client.exe" (
            set "CLIENT_EXE=%%~fT\%%~R\apilevel1_containers_client.exe"
            goto launch_example
        )
    )
)

echo Unable to find apilevel1_containers_client.exe. Build the API LEVEL 1 example first.
exit /b 1

:launch_example
for %%I in ("%CLIENT_EXE%") do set "CLIENT_DIR=%%~dpI"
call "%SCRIPT_DIR%Start-MfdWindow.bat" "%WINDOW_FILE%" --runtime-dir "%CLIENT_DIR%"
if errorlevel 1 exit /b %ERRORLEVEL%

timeout /t 1 /nobreak >nul
start "" /D "%CLIENT_DIR%" "%CLIENT_EXE%"
exit /b %ERRORLEVEL%
