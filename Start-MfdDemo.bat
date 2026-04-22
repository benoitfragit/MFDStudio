@echo off
setlocal
call "%~dp0Start-MfdWindow.bat" "assets/windows/demo_pages.json" %*
exit /b %ERRORLEVEL%
