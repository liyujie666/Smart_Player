@echo off
chcp 65001 >nul
echo ========================================
echo   Smart Player - Clean Build
echo ========================================
echo.

set SCRIPT_DIR=%~dp0
set BUILD_DIR=%SCRIPT_DIR%..\build-cmake

if exist "%BUILD_DIR%" (
    echo Cleaning: %BUILD_DIR%
    rd /s /q "%BUILD_DIR%"
    echo [+] Build directory removed.
) else (
    echo Build directory does not exist.
)

echo.
echo Done.
pause
