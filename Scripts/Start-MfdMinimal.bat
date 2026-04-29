@echo off
setlocal
set "MFD_DEFAULT_FRAMEBUFFER_PLUGIN=mfd_framebuffer_stdout_plugin.dll"
call "%~dp0Start-MfdWindow.bat" "assets/windows/demo_pages_minimal.json" %*
exit /b %ERRORLEVEL%
