@echo off
setlocal
rem A dropped ISO is passed as the positional ISO argument; named flags still work.
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0build.ps1" %*
set "ac4_exit=%errorlevel%"
if not "%AC4_NO_PAUSE%"=="1" pause
exit /b %ac4_exit%
