@echo off
echo ================================================
echo RCKangaroo Server - Windows Setup Script
echo ================================================
echo.

:: Set colors
set RED=[91m
set GREEN=[92m
set YELLOW=[93m
set NC=[0m

echo %GREEN%[INFO]%NC% Setting up RCKangaroo Server for Windows...
echo.

:: Check Python installation
echo %GREEN%[INFO]%NC% Checking Python installation...
python --version >nul 2>&1
if %errorLevel% neq 0 (
    python3 --version >nul 2>&1
    if %errorLevel% neq 0 (
        echo %RED%[ERROR]%NC% Python not found!
        echo Please install Python 3.7+ from https://python.org/downloads/
        echo Make sure to check "Add Python to PATH" during installation.
        pause
        exit /b 1
    ) else (
        set PYTHON_CMD=python3
        echo %GREEN%[INFO]%NC% Found Python 3
    )
) else (
    set PYTHON_CMD=python
    echo %GREEN%[INFO]%NC% Found Python
)

:: Show Python version
%PYTHON_CMD% --version

:: Check pip
echo %GREEN%[INFO]%NC% Checking pip...
%PYTHON_CMD% -m pip --version >nul 2>&1
if %errorLevel% neq 0 (
    echo %RED%[ERROR]%NC% pip not found!
    echo Please install pip or reinstall Python with pip included.
    pause
    exit /b 1
)
echo %GREEN%[INFO]%NC% pip found

:: Upgrade pip
echo %GREEN%[INFO]%NC% Upgrading pip...
%PYTHON_CMD% -m pip install --upgrade pip

:: Install dependencies
echo %GREEN%[INFO]%NC% Installing Python dependencies...
echo %YELLOW%[INFO]%NC% This may take a few minutes...

%PYTHON_CMD% -m pip install flask
if %errorLevel% neq 0 (
    echo %RED%[ERROR]%NC% Failed to install Flask
    pause
    exit /b 1
)

%PYTHON_CMD% -m pip install requests
if %errorLevel% neq 0 (
    echo %RED%[ERROR]%NC% Failed to install requests
    pause
    exit /b 1
)

:: Check firewall
echo %GREEN%[INFO]%NC% Checking Windows Firewall...
echo %YELLOW%[WARNING]%NC% You may need to allow Python through Windows Firewall
echo when prompted, or the server won't be accessible from other machines.

:: Create start script
echo %GREEN%[INFO]%NC% Creating start scripts...

:: Create basic start script
echo @echo off > start_server.bat
echo echo Starting RCKangaroo Server... >> start_server.bat
echo %PYTHON_CMD% kangaroo_server.py >> start_server.bat
echo pause >> start_server.bat

:: Create start script with custom settings
echo @echo off > start_server_custom.bat
echo echo Starting RCKangaroo Server with custom settings... >> start_server_custom.bat
echo echo Usage: start_server_custom.bat [host] [port] >> start_server_custom.bat
echo echo Example: start_server_custom.bat 0.0.0.0 8080 >> start_server_custom.bat
echo echo. >> start_server_custom.bat
echo set HOST=%%1 >> start_server_custom.bat
echo set PORT=%%2 >> start_server_custom.bat
echo if "%%HOST%%"=="" set HOST=127.0.0.1 >> start_server_custom.bat
echo if "%%PORT%%"=="" set PORT=8080 >> start_server_custom.bat
echo echo Starting server on %%HOST%%:%%PORT%%... >> start_server_custom.bat
echo %PYTHON_CMD% kangaroo_server.py --host %%HOST%% --port %%PORT%% >> start_server_custom.bat
echo pause >> start_server_custom.bat

:: Create configuration script
echo @echo off > configure_search.bat
echo echo ================================================ >> configure_search.bat
echo echo RCKangaroo Server - Search Configuration >> configure_search.bat
echo echo ================================================ >> configure_search.bat
echo echo. >> configure_search.bat
echo echo This script will help you configure the search parameters. >> configure_search.bat
echo echo. >> configure_search.bat
echo echo Examples: >> configure_search.bat
echo echo   Bitcoin Puzzle #85: >> configure_search.bat
echo echo     Start: 0x1000000000000000000000 >> configure_search.bat
echo echo     End:   0x1FFFFFFFFFFFFFFFFFFFFF >> configure_search.bat
echo echo     PubKey: 0329c4574a4fd8c810b7e42a4b398882b381bcd85e40c6883712912d167c83e73a >> configure_search.bat
echo echo     DP: 16, Range Size: 0x100000000000000 >> configure_search.bat
echo echo. >> configure_search.bat
echo set /p SERVER_URL="Enter server URL (default: http://localhost:8080): " >> configure_search.bat
echo if "%%SERVER_URL%%"=="" set SERVER_URL=http://localhost:8080 >> configure_search.bat
echo echo. >> configure_search.bat
echo set /p START_RANGE="Enter start range (hex): " >> configure_search.bat
echo set /p END_RANGE="Enter end range (hex): " >> configure_search.bat
echo set /p PUBKEY="Enter public key (hex): " >> configure_search.bat
echo set /p DP_BITS="Enter DP bits (14-22): " >> configure_search.bat
echo set /p RANGE_SIZE="Enter range size per client (hex): " >> configure_search.bat
echo echo. >> configure_search.bat
echo echo Configuring server... >> configure_search.bat
echo curl -X POST %%SERVER_URL%%/api/configure -H "Content-Type: application/json" -d "{\"start_range\": \"%%START_RANGE%%\", \"end_range\": \"%%END_RANGE%%\", \"pubkey\": \"%%PUBKEY%%\", \"dp_bits\": %%DP_BITS%%, \"range_size\": \"%%RANGE_SIZE%%\"}" >> configure_search.bat
echo echo. >> configure_search.bat
echo echo Configuration completed! >> configure_search.bat
echo pause >> configure_search.bat

echo %GREEN%[INFO]%NC% Setup completed successfully!
echo.
echo Created scripts:
echo   start_server.bat          - Start server with default settings
echo   start_server_custom.bat   - Start server with custom host/port
echo   configure_search.bat      - Configure search parameters
echo.
echo Next steps:
echo   1. Run: start_server.bat
echo   2. Configure search: configure_search.bat
echo   3. Start clients from other machines
echo.
echo %YELLOW%[NOTE]%NC% Make sure to allow Python through Windows Firewall
echo when prompted if you want to accept connections from other machines.
echo.
pause