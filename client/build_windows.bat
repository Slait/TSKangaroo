@echo off
echo ================================================
echo RCKangaroo Client - Windows Build Script
echo ================================================
echo.

:: Set colors
set RED=[91m
set GREEN=[92m
set YELLOW=[93m
set NC=[0m

echo %GREEN%[INFO]%NC% Building RCKangaroo Client for Windows...
echo.

:: Find MSBuild
set MSBUILD_PATH=
if exist "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\MSBuild\Current\Bin\MSBuild.exe" (
    set MSBUILD_PATH=C:\Program Files\Microsoft Visual Studio\2022\Enterprise\MSBuild\Current\Bin\MSBuild.exe
) else if exist "C:\Program Files\Microsoft Visual Studio\2022\Professional\MSBuild\Current\Bin\MSBuild.exe" (
    set MSBUILD_PATH=C:\Program Files\Microsoft Visual Studio\2022\Professional\MSBuild\Current\Bin\MSBuild.exe
) else if exist "C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe" (
    set MSBUILD_PATH=C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe
) else if exist "C:\Program Files (x86)\Microsoft Visual Studio\2019\Enterprise\MSBuild\Current\Bin\MSBuild.exe" (
    set MSBUILD_PATH=C:\Program Files (x86)\Microsoft Visual Studio\2019\Enterprise\MSBuild\Current\Bin\MSBuild.exe
) else if exist "C:\Program Files (x86)\Microsoft Visual Studio\2019\Professional\MSBuild\Current\Bin\MSBuild.exe" (
    set MSBUILD_PATH=C:\Program Files (x86)\Microsoft Visual Studio\2019\Professional\MSBuild\Current\Bin\MSBuild.exe
) else if exist "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\MSBuild\Current\Bin\MSBuild.exe" (
    set MSBUILD_PATH=C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\MSBuild\Current\Bin\MSBuild.exe
)

if "%MSBUILD_PATH%"=="" (
    echo %RED%[ERROR]%NC% MSBuild not found!
    echo Please install Visual Studio 2019 or 2022 with C++ support.
    pause
    exit /b 1
)

echo %GREEN%[INFO]%NC% Using MSBuild: %MSBUILD_PATH%

:: Check dependencies
echo %GREEN%[INFO]%NC% Checking dependencies...

if "%VCPKG_ROOT%"=="" (
    if exist "C:\vcpkg" (
        set VCPKG_ROOT=C:\vcpkg
    ) else (
        echo %RED%[ERROR]%NC% vcpkg not found!
        echo Please run setup_windows.bat first.
        pause
        exit /b 1
    )
)

if not exist "%VCPKG_ROOT%\installed\x64-windows\lib\libcurl.lib" (
    echo %RED%[ERROR]%NC% libcurl not found!
    echo Please run setup_windows.bat first.
    pause
    exit /b 1
)

if not exist "%VCPKG_ROOT%\installed\x64-windows\lib\jsoncpp.lib" (
    echo %RED%[ERROR]%NC% jsoncpp not found!
    echo Please run setup_windows.bat first.
    pause
    exit /b 1
)

echo %GREEN%[INFO]%NC% Dependencies check passed.

:: Clean previous build
echo %GREEN%[INFO]%NC% Cleaning previous build...
if exist "x64" rmdir /s /q "x64"

:: Build Debug configuration
echo %GREEN%[INFO]%NC% Building Debug configuration...
"%MSBUILD_PATH%" RCKangarooClient.sln /p:Configuration=Debug /p:Platform=x64 /m
if %errorLevel% neq 0 (
    echo %RED%[ERROR]%NC% Debug build failed!
    pause
    exit /b 1
)

:: Build Release configuration
echo %GREEN%[INFO]%NC% Building Release configuration...
"%MSBUILD_PATH%" RCKangarooClient.sln /p:Configuration=Release /p:Platform=x64 /m
if %errorLevel% neq 0 (
    echo %RED%[ERROR]%NC% Release build failed!
    pause
    exit /b 1
)

echo.
echo %GREEN%[SUCCESS]%NC% Build completed successfully!
echo.
echo Output files:
echo   Debug:   x64\Debug\RCKangarooClient.exe
echo   Release: x64\Release\RCKangarooClient.exe
echo.
echo Test the build:
echo   x64\Release\RCKangarooClient.exe -server http://localhost:8080
echo.
pause