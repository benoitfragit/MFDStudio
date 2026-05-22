@echo off
setlocal
call "%~dp0Start-MfdWindow.bat" "assets/windows/package_test_window.json" %*
exit /b %ERRORLEVEL%
