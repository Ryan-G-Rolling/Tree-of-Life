@echo off
setlocal
set "PATH=%SystemRoot%\System32;D:\mingw64\bin;%PATH%"
set "GCC=D:\mingw64\bin\gcc.exe"
if not exist "%GCC%" set "GCC=gcc"

"%GCC%" --version >nul 2>nul
if errorlevel 1 (
    echo [ERROR] gcc not found. Edit build.bat and point GCC to your gcc.exe.
    exit /b 1
)

if /i "%1"=="clean" (
    if exist build rmdir /s /q build
    if exist treeoflife.exe del /q treeoflife.exe
    del /q shot_*.png 2>nul
    echo cleaned: build\ treeoflife.exe shot_*.png
    exit /b 0
)

if not exist build mkdir build

"%GCC%" -std=c11 -Wall -Wextra -Wno-unused-parameter -O2 -Iinclude -Idata src\main.c src\ui.c src\render.c src\screens.c src\input.c src\atmosphere.c src\tex.c src\ui_adapter.c src\game.c src\economy.c src\save.c src\evolution.c src\species.c src\environment.c src\prestige.c src\legacy.c -o treeoflife.exe -lraylib -lopengl32 -lgdi32 -lwinmm -lm
if errorlevel 1 (
    echo [ERROR] build failed.
    exit /b 1
)

echo build ok: treeoflife.exe

if /i "%1"=="shot" (
    echo exporting 14 shots at 3840x2160 ...
    treeoflife.exe --shot --ss 3 > shot_log.txt 2>&1
    echo shots done, see shot_log.txt
)
