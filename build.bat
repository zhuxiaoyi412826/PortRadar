@echo off
title PortLens Build Script

REM ============================================================
REM  If g++ is not in your PATH, set your MinGW-w64 bin path here:
set "GCC_PATH=D:\software\GCC\winlibs-x86_64-posix-seh-gcc-15.2.0-mingw-w64msvcrt-13.0.0-r6\mingw64\bin"
REM ============================================================

where g++ >nul 2>&1
if %errorlevel% equ 0 goto :found

if not exist "%GCC_PATH%\g++.exe" (
    echo [ERROR] GCC not found!
    echo.
    echo Install MinGW-w64 GCC and add it to PATH,
    echo or edit GCC_PATH at the top of build.bat
    echo.
    pause
    exit /b 1
)
set "PATH=%GCC_PATH%;%PATH%"

:found
echo ================================================
echo   PortLens - Port Checker C++ Build
echo ================================================
echo.
echo [INFO] GCC ready
echo [INFO] Compiling (static link)...
echo.

if exist port_checker.exe del /q port_checker.exe

g++ -Os -s -std=c++17 -static -static-libgcc -static-libstdc++ main.cpp port_checker.cpp process_manager.cpp -o port_checker.exe -lws2_32 -liphlpapi -lpsapi -lversion -lshell32 -mconsole

if %errorlevel% neq 0 (
    echo.
    echo [ERROR] Build failed!
    pause
    exit /b 1
)

echo.
echo ================================================
echo   Build successful!
echo ================================================
echo.

for %%f in (port_checker.exe) do (
    echo   File: %%~nf%%~xf
    echo   Size: %%~zf bytes
)

echo.
echo   Press any key to run...
pause >nul
echo.

port_checker.exe
