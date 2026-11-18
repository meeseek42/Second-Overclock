@echo off
setlocal
pwsh.exe -NoProfile -File "%~dp0Generate-DORAssets.ps1"
set "exitCode=%errorlevel%"
endlocal & exit /b %exitCode%
