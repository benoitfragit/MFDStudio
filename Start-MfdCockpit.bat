@echo off
setlocal
call "%~dp0Start-MfdWindow.bat" "assets/windows/demo_pages_cockpit.json" %*
exit /b %ERRORLEVEL%
