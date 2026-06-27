@echo off
chcp 65001 >nul
echo ========================================
echo   Smart Player - CMake Build Script
echo ========================================
echo.

:: ===== 关键：使用 Qt 自带的 MinGW 工具链 =====
:: D:\Qt\Tools\mingw1310_64 是与 Qt 6.9.0 mingw_64 一致的 MSVCRT 工具链
:: （不能用 msys2 的 ucrt64，否则链接会失败）
set MINGW_PATH=D:\Qt\Tools\mingw1310_64\bin

:: 把 MinGW 路径放到 PATH 最前面
set PATH=%MINGW_PATH%;%PATH%

:: 验证编译器
where.exe g++.exe >nul 2>&1
if %ERRORLEVEL% neq 0 (
    echo [ERROR] g++.exe not found in PATH!
    echo        Make sure %MINGW_PATH% exists.
    pause
    exit /b 1
)

:: 获取项目根目录（脚本在 scripts/ 下，回到上一层）
set SCRIPT_DIR=%~dp0
set SOURCE_DIR=%SCRIPT_DIR%..
set BUILD_DIR=%SOURCE_DIR%\build-cmake

:: 创建构建目录
if not exist "%BUILD_DIR%" (
    mkdir "%BUILD_DIR%"
    echo [+] Created build directory: %BUILD_DIR%
)

echo.
echo Building in: %BUILD_DIR%
echo Using compiler: 
g++.exe --version | findstr /C:"g++.exe"
echo.

cd /d "%BUILD_DIR%"

:: CMake 配置
echo [1/2] Running CMake configuration...
cmake -G "MinGW Makefiles" ^
    -DCMAKE_BUILD_TYPE=Release ^
    -DCMAKE_C_COMPILER="%MINGW_PATH%\gcc.exe" ^
    -DCMAKE_CXX_COMPILER="%MINGW_PATH%\g++.exe" ^
    -DCMAKE_MAKE_PROGRAM="%MINGW_PATH%\mingw32-make.exe" ^
    -DQt6_DIR="C:\Qt\6.9.0\mingw_64\lib\cmake\Qt6" ^
    "%SOURCE_DIR%"

if %ERRORLEVEL% neq 0 (
    echo.
    echo [ERROR] CMake configuration failed!
    pause
    exit /b 1
)

echo.
echo [2/2] Building project...
echo.

:: 确保没有任何残留的 Smart_Player.exe 占用输出文件
taskkill /F /IM Smart_Player.exe /T >nul 2>&1

:: 编译
cmake --build . --parallel

if %ERRORLEVEL% neq 0 (
    echo.
    echo [ERROR] Build failed!
    pause
    exit /b 1
)

echo.
echo ========================================
echo   Build completed successfully!
echo ========================================
echo.
echo Executable: %BUILD_DIR%\Smart_Player.exe
echo.
pause
