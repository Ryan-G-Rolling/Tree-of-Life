@echo off
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0build.ps1"
if errorlevel 1 exit /b 1
if /i "%1"=="shot" "%~dp0treeoflife.exe" --shot --ss 3
