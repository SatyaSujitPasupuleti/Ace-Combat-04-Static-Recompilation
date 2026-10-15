@echo off
setlocal
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0build.ps1" -Action Play %*
set "ac4_exit=%errorlevel%"
if not "%ac4_exit%"=="0" if not "%AC4_NO_PAUSE%"=="1" pause
exit /b %ac4_exit%
