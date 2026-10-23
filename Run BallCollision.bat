@echo off
rem Double-click to play. Builds the game first if it has not been built yet,
rem then starts it from this folder so it reads the config.ini next to this file.
cd /d "%~dp0"

if not exist "build\bin\BallCollision.exe" (
    echo BallCollision has not been built yet. Building it now...
    if exist "D:\Tools\WinLibs\mingw64\bin\g++.exe" (
        powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0build.ps1" -ToolchainDirectory "D:\Tools\WinLibs\mingw64\bin"
    ) else (
        powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0build.ps1"
    )
    if errorlevel 1 (
        echo.
        echo The build failed. See the messages above.
        pause
        exit /b 1
    )
)

start "" "build\bin\BallCollision.exe"
