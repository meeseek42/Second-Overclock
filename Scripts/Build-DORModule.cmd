@echo off
set "USERPROFILE=R:\Saved\DORUEUser"
set "APPDATA=R:\Saved\DORUEUser\AppData\Roaming"
set "LOCALAPPDATA=R:\Saved\DORUEUser\AppData\Local"
set "TEMP=R:\Saved\DORUEUser\Temp"
set "TMP=R:\Saved\DORUEUser\Temp"
if not exist "%APPDATA%" mkdir "%APPDATA%"
if not exist "%LOCALAPPDATA%" mkdir "%LOCALAPPDATA%"
if not exist "%TEMP%" mkdir "%TEMP%"
call "C:\Program Files\Epic Games\UE_4.27\Engine\Build\BatchFiles\Build.bat" FSDEditor Win64 Development R:\FSD.uproject -WaitMutex -NoHotReload
exit /b %errorlevel%
