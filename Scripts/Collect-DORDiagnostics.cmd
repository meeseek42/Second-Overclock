@echo off
setlocal
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0Collect-DORDiagnostics.ps1"
echo.
pause
