@echo off
echo ================================================
echo RCKangaroo Client - Windows Setup Script
echo ================================================
echo.

:: Check if running as administrator
net session >nul 2>&1
if %errorLevel% neq 0 (
    echo This script requires administrator privileges.
    echo Please run as administrator.
    pause
    exit /b 1
)

:: Set colors
set RED=[91m
set GREEN=[92m
set YELLOW=[93m
set NC=[0m

echo %GREEN%[INFO]%NC% Setting up RCKangaroo Client for Windows...
echo.

:: Check Visual Studio
echo %GREEN%[INFO]%NC% Checking Visual Studio installation...
if not exist "C:\Program Files\Microsoft Visual Studio\2022" (
    if not exist "C:\Program Files (x86)\Microsoft Visual Studio\2019" (
        echo %RED%[ERROR]%NC% Visual Studio 2019/2022 not found!
        echo Please install Visual Studio 2019 or 2022 with C++ support.
        pause
        exit /b 1
    )
)
echo %GREEN%[INFO]%NC% Visual Studio found.

:: Check CUDA
echo %GREEN%[INFO]%NC% Checking CUDA installation...
if "%CUDA_PATH%"=="" (
    if exist "C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v12.6" (
        set CUDA_PATH=C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v12.6
    ) else if exist "C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v12.5" (
        set CUDA_PATH=C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v12.5
    ) else if exist "C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v12.4" (
        set CUDA_PATH=C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v12.4
    ) else if exist "C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v11.8" (
        set CUDA_PATH=C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v11.8
    ) else (
        echo %RED%[ERROR]%NC% CUDA Toolkit not found!
        echo Please install CUDA Toolkit 11.8 or later from:
        echo https://developer.nvidia.com/cuda-downloads
        pause
        exit /b 1
    )
)
echo %GREEN%[INFO]%NC% CUDA found at: %CUDA_PATH%

:: Check/Install vcpkg
echo %GREEN%[INFO]%NC% Checking vcpkg package manager...
if not exist "C:\vcpkg" (
    echo %YELLOW%[INFO]%NC% Installing vcpkg...
    cd C:\
    git clone https://github.com/Microsoft/vcpkg.git
    cd vcpkg
    call bootstrap-vcpkg.bat
    vcpkg integrate install
) else (
    echo %GREEN%[INFO]%NC% vcpkg found.
    cd C:\vcpkg
)

:: Set VCPKG_ROOT environment variable
setx VCPKG_ROOT "C:\vcpkg" /M >nul

:: Install dependencies
echo %GREEN%[INFO]%NC% Installing dependencies via vcpkg...
echo %YELLOW%[INFO]%NC% This may take 10-30 minutes depending on your system...

vcpkg install curl:x64-windows
if %errorLevel% neq 0 (
    echo %RED%[ERROR]%NC% Failed to install libcurl
    pause
    exit /b 1
)

vcpkg install jsoncpp:x64-windows
if %errorLevel% neq 0 (
    echo %RED%[ERROR]%NC% Failed to install jsoncpp
    pause
    exit /b 1
)

echo %GREEN%[INFO]%NC% Dependencies installed successfully!
echo.

:: Return to client directory
cd /d "%~dp0"

echo %GREEN%[INFO]%NC% Setup completed successfully!
echo.
echo Next steps:
echo 1. Open RCKangarooClient.sln in Visual Studio
echo 2. Select Release x64 configuration
echo 3. Build Solution (Ctrl+Shift+B)
echo.
echo Or use the build script:
echo   build_windows.bat
echo.
pause