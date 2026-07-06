@echo off
setlocal
set "WINDOW_FILE=%~dp0..\examples\hud\assets\windows\hud_window.json"
if not exist "%WINDOW_FILE%" set "WINDOW_FILE=assets\windows\hud_window.json"
call "%~dp0Start-MfdWindow.bat" "%WINDOW_FILE%" %*
exit /b %ERRORLEVEL%
