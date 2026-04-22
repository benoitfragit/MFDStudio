@echo off
setlocal
set "MFD_DEFAULT_FRAMEBUFFER_PLUGIN=mfd_framebuffer_stdout_plugin.dll"
call "%~dp0Start-MfdWindow.bat" "assets/windows/mfd_tutorial.json" %*
exit /b %ERRORLEVEL%
