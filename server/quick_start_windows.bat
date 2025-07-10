@echo off
setlocal enabledelayedexpansion

title RCKangaroo Server - Windows Quick Start

:: Set colors
set RED=[91m
set GREEN=[92m
set YELLOW=[93m
set BLUE=[94m
set CYAN=[96m
set NC=[0m

echo.
echo %CYAN%================================================%NC%
echo %CYAN%      RCKangaroo Server - Windows Quick Start  %NC%
echo %CYAN%================================================%NC%
echo.
echo %GREEN%[INFO]%NC% This script will help you set up and run the RCKangaroo server
echo %GREEN%[INFO]%NC% for distributed Bitcoin private key searching.
echo.

:: Check if running in correct directory
if not exist "kangaroo_server.py" (
    echo %RED%[ERROR]%NC% kangaroo_server.py not found!
    echo Please run this script from the server/ directory.
    echo.
    pause
    exit /b 1
)

:: Step 1: Check Python
echo %BLUE%[STEP 1]%NC% Checking Python installation...
python --version >nul 2>&1
if %errorLevel% neq 0 (
    python3 --version >nul 2>&1
    if %errorLevel% neq 0 (
        echo %RED%[ERROR]%NC% Python not found!
        echo.
        echo Please install Python 3.7+ from: https://python.org/downloads/
        echo %YELLOW%Important:%NC% Make sure to check "Add Python to PATH" during installation.
        echo.
        echo After installing Python, restart this script.
        pause
        exit /b 1
    ) else (
        set PYTHON_CMD=python3
    )
) else (
    set PYTHON_CMD=python
)

%PYTHON_CMD% --version
echo %GREEN%[OK]%NC% Python found!
echo.

:: Step 2: Install dependencies
echo %BLUE%[STEP 2]%NC% Installing Python dependencies...
echo %YELLOW%[INFO]%NC% Installing Flask and requests...

%PYTHON_CMD% -m pip install flask requests >nul 2>&1
if %errorLevel% neq 0 (
    echo %RED%[ERROR]%NC% Failed to install dependencies!
    echo Trying to install with user flag...
    %PYTHON_CMD% -m pip install --user flask requests
    if %errorLevel% neq 0 (
        echo %RED%[ERROR]%NC% Installation failed. Please check your Python/pip setup.
        pause
        exit /b 1
    )
)

echo %GREEN%[OK]%NC% Dependencies installed!
echo.

:: Step 3: Start server
echo %BLUE%[STEP 3]%NC% Starting the server...
echo %YELLOW%[INFO]%NC% The server will run on http://localhost:8080
echo %YELLOW%[INFO]%NC% You can access it from other computers using your IP address.
echo.

:: Check if port is available
netstat -an | findstr :8080 >nul 2>&1
if %errorLevel% equ 0 (
    echo %YELLOW%[WARNING]%NC% Port 8080 is already in use!
    set /p USE_DIFFERENT_PORT="Use a different port? (y/N): "
    if /i "!USE_DIFFERENT_PORT!"=="y" (
        set /p NEW_PORT="Enter port number (8081): "
        if "!NEW_PORT!"=="" set NEW_PORT=8081
        set SERVER_PORT=!NEW_PORT!
    ) else (
        echo Attempting to use port 8080 anyway...
        set SERVER_PORT=8080
    )
) else (
    set SERVER_PORT=8080
)

echo %GREEN%[INFO]%NC% Starting server on port %SERVER_PORT%...
echo %YELLOW%[INFO]%NC% Press Ctrl+C to stop the server.
echo.

:: Start server in background
start "RCKangaroo Server" /MIN %PYTHON_CMD% kangaroo_server.py --host 0.0.0.0 --port %SERVER_PORT%

:: Wait for server to start
echo %GREEN%[INFO]%NC% Waiting for server to start...
timeout /t 3 /nobreak >nul

:: Check if server is running
curl -s http://localhost:%SERVER_PORT%/api/status >nul 2>&1
if %errorLevel% neq 0 (
    echo %RED%[ERROR]%NC% Server failed to start!
    echo Check the server window for error messages.
    pause
    exit /b 1
)

echo %GREEN%[SUCCESS]%NC% Server is running!
echo.

:: Step 4: Configuration options
echo %BLUE%[STEP 4]%NC% Server Configuration
echo.
echo Choose configuration option:
echo   1. Quick setup for Bitcoin Puzzle #85 (recommended for testing)
echo   2. Bitcoin Puzzle #64 (solved, good for quick testing)
echo   3. Manual configuration
echo   4. Skip configuration (configure later)
echo.
set /p CONFIG_CHOICE="Enter your choice (1): "
if "%CONFIG_CHOICE%"=="" set CONFIG_CHOICE=1

if "%CONFIG_CHOICE%"=="1" (
    echo %GREEN%[INFO]%NC% Configuring for Bitcoin Puzzle #85...
    curl -X POST http://localhost:%SERVER_PORT%/api/configure ^
      -H "Content-Type: application/json" ^
      -d "{\"start_range\": \"0x1000000000000000000000\", \"end_range\": \"0x1FFFFFFFFFFFFFFFFFFFFF\", \"pubkey\": \"0329c4574a4fd8c810b7e42a4b398882b381bcd85e40c6883712912d167c83e73a\", \"dp_bits\": 16, \"range_size\": \"0x100000000000000\"}" ^
      >nul 2>&1
    
    if !errorLevel! equ 0 (
        echo %GREEN%[SUCCESS]%NC% Configured for Bitcoin Puzzle #85
    ) else (
        echo %YELLOW%[WARNING]%NC% Configuration may have failed
    )
    
) else if "%CONFIG_CHOICE%"=="2" (
    echo %GREEN%[INFO]%NC% Configuring for Bitcoin Puzzle #64 (testing)...
    curl -X POST http://localhost:%SERVER_PORT%/api/configure ^
      -H "Content-Type: application/json" ^
      -d "{\"start_range\": \"0x8000000000000000\", \"end_range\": \"0xFFFFFFFFFFFFFFFE\", \"pubkey\": \"0320d8e8133df7eca2dc8c8b4cf2c50b05eeb2c5b6594518194ccca95e45bb19e0e8\", \"dp_bits\": 14, \"range_size\": \"0x10000000000000\"}" ^
      >nul 2>&1
      
    if !errorLevel! equ 0 (
        echo %GREEN%[SUCCESS]%NC% Configured for Bitcoin Puzzle #64 (testing)
    ) else (
        echo %YELLOW%[WARNING]%NC% Configuration may have failed
    )
    
) else if "%CONFIG_CHOICE%"=="3" (
    echo %GREEN%[INFO]%NC% Run configure_bitcoin_puzzle.bat for manual configuration
    
) else (
    echo %YELLOW%[INFO]%NC% Skipping configuration. Run configure_bitcoin_puzzle.bat later.
)

echo.

:: Step 5: Show access information
echo %BLUE%[STEP 5]%NC% Access Information
echo.

:: Get local IP addresses
echo %GREEN%[SERVER ACCESS]%NC%
echo Local access:    http://localhost:%SERVER_PORT%
echo Web interface:   http://localhost:%SERVER_PORT%
echo API status:      http://localhost:%SERVER_PORT%/api/status

:: Try to get IP address
for /f "tokens=2 delims=:" %%a in ('ipconfig ^| findstr "IPv4"') do (
    set IP=%%a
    set IP=!IP: =!
    if defined IP (
        echo Network access:   http://!IP!:%SERVER_PORT%
        goto :ip_found
    )
)
:ip_found

echo.

:: Step 6: Show client information
echo %BLUE%[CLIENT SETUP]%NC%
echo.
echo To connect GPU clients to this server:
echo.
echo 1. %YELLOW%Build the client:%NC%
echo    - Windows: Use build_windows.bat in client/ folder
echo    - Linux: Use make in client/ folder
echo.
echo 2. %YELLOW%Run client in server mode:%NC%
echo    RCKangarooClient.exe -s http://YOUR_SERVER_IP:%SERVER_PORT%
echo.
echo 3. %YELLOW%Example for this server:%NC%
if defined IP (
    echo    RCKangarooClient.exe -s http://!IP!:%SERVER_PORT%
)
echo    RCKangarooClient.exe -s http://localhost:%SERVER_PORT%
echo.

:: Step 7: Firewall reminder
echo %BLUE%[FIREWALL SETUP]%NC%
echo.
echo %YELLOW%[IMPORTANT]%NC% If you want to accept connections from other computers:
echo 1. Windows may show a firewall dialog - click "Allow access"
echo 2. Or manually allow Python through Windows Firewall
echo 3. Make sure port %SERVER_PORT% is accessible on your network
echo.

:: Step 8: Monitoring
echo %BLUE%[MONITORING]%NC%
echo.
echo Monitor your server using:
echo - Web interface: http://localhost:%SERVER_PORT%
echo - API endpoint:  curl http://localhost:%SERVER_PORT%/api/status
echo - Server logs in the RCKangaroo Server window
echo.

:: Step 9: Final instructions
echo %CYAN%================================================%NC%
echo %GREEN%[SETUP COMPLETE]%NC%
echo %CYAN%================================================%NC%
echo.
echo %GREEN%✓%NC% Server is running on port %SERVER_PORT%
if "%CONFIG_CHOICE%"=="1" echo %GREEN%✓%NC% Configured for Bitcoin Puzzle #85
if "%CONFIG_CHOICE%"=="2" echo %GREEN%✓%NC% Configured for Bitcoin Puzzle #64 (testing)
echo %GREEN%✓%NC% Ready to accept client connections
echo.
echo %BLUE%Next steps:%NC%
echo 1. Open http://localhost:%SERVER_PORT% in your browser
echo 2. Build and run clients on GPU machines
echo 3. Monitor progress and wait for solution
echo.
echo %YELLOW%Useful files created:%NC%
echo - start_server.bat           (simple restart)
echo - configure_bitcoin_puzzle.bat (reconfigure)
echo - kangaroo_server.db         (database - backup regularly!)
echo.
echo %YELLOW%To stop the server:%NC% Close the "RCKangaroo Server" window or press Ctrl+C
echo.

:: Keep window open
echo Press any key to exit this setup window...
echo (The server will continue running in the background)
pause >nul