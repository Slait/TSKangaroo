@echo off
setlocal enabledelayedexpansion

echo ============================================================
echo RCKangaroo Server - Bitcoin Puzzle Configuration
echo ============================================================
echo.

:: Set colors
set RED=[91m
set GREEN=[92m
set YELLOW=[93m
set BLUE=[94m
set NC=[0m

echo %GREEN%[INFO]%NC% This script will configure the server for Bitcoin Puzzles.
echo.

:: Check if server is running
echo %GREEN%[INFO]%NC% Checking if server is running...
curl -s http://localhost:8080/api/status >nul 2>&1
if %errorLevel% neq 0 (
    echo %RED%[ERROR]%NC% Server is not running!
    echo Please start the server first using: start_server.bat
    pause
    exit /b 1
)
echo %GREEN%[INFO]%NC% Server is running

:: Puzzle selection menu
echo.
echo %BLUE%[SELECT PUZZLE]%NC%
echo Available Bitcoin Puzzles:
echo.
echo 64  - Puzzle #64  (SOLVED - for testing only)
echo 85  - Puzzle #85  (Active, 22+ digits) 
echo 130 - Puzzle #130 (Active, 34+ digits)
echo 135 - Puzzle #135 (Active, 35+ digits)
echo 160 - Puzzle #160 (Active, 42+ digits)
echo.
echo Or enter custom puzzle number (64-160)
echo.
set /p PUZZLE="Enter puzzle number (85): "
if "%PUZZLE%"=="" set PUZZLE=85

:: Puzzle configurations
if "%PUZZLE%"=="64" (
    set START=0x8000000000000000
    set END=0xFFFFFFFFFFFFFFFE
    set PUBKEY=0320d8e8133df7eca2dc8c8b4cf2c50b05eeb2c5b6594518194ccca95e45bb19e0e8
    set DP_BITS=14
    set RANGE_SIZE=0x10000000000000
    echo %YELLOW%[WARNING]%NC% Puzzle #64 is already solved! Use for testing only.
) else if "%PUZZLE%"=="85" (
    set START=0x1000000000000000000000
    set END=0x1FFFFFFFFFFFFFFFFFFFFF
    set PUBKEY=0329c4574a4fd8c810b7e42a4b398882b381bcd85e40c6883712912d167c83e73a
    set DP_BITS=16
    set RANGE_SIZE=0x100000000000000
) else if "%PUZZLE%"=="130" (
    set START=0x20000000000000000000000000000000
    set END=0x3FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFD
    set PUBKEY=0321dfeba56b6de39432dd5ca8e9fb94c46e0db2c9dad50f40b4b0f4bbd6e222f2b3
    set DP_BITS=18
    set RANGE_SIZE=0x100000000000000000000000
) else if "%PUZZLE%"=="135" (
    set START=0x400000000000000000000000000000000
    set END=0x7FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF
    set PUBKEY=032c74ae09ed4845fb6ad38e7d7ba7a6b27b7b2a65baf5045e55a0c0ce606fe3ed41
    set DP_BITS=18
    set RANGE_SIZE=0x200000000000000000000000
) else if "%PUZZLE%"=="160" (
    set START=0x800000000000000000000000000000000000000
    set END=0xFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFE
    set PUBKEY=03c16ff3c0e1c23b7efc6bb47dbb5c2a8d6d3d969b58b1c4b6dd57eab8d2b39ccdc9
    set DP_BITS=20
    set RANGE_SIZE=0x1000000000000000000000000000000
) else (
    echo %RED%[ERROR]%NC% Unknown puzzle number: %PUZZLE%
    echo Please use a valid puzzle number or check the documentation.
    pause
    exit /b 1
)

:: Show configuration
echo.
echo %BLUE%[CONFIGURATION]%NC%
echo Puzzle Number: %PUZZLE%
echo Start Range:   !START!
echo End Range:     !END!
echo Public Key:    !PUBKEY!
echo DP Bits:       !DP_BITS!
echo Range Size:    !RANGE_SIZE!
echo.

:: Calculate expected statistics
if "%PUZZLE%"=="64" (
    echo %YELLOW%Expected time with 1 RTX 4090: ~1 hour (already solved)%NC%
) else if "%PUZZLE%"=="85" (
    echo %YELLOW%Expected time with 1 RTX 4090: ~6 months%NC%
    echo %YELLOW%Expected time with 10 RTX 4090: ~20 days%NC%
    echo %YELLOW%Expected time with 100 RTX 4090: ~2 days%NC%
) else if "%PUZZLE%"=="130" (
    echo %YELLOW%Expected time: Very long (thousands of years)%NC%
) else (
    echo %YELLOW%Expected time: Extremely long%NC%
)
echo.

:: Confirm configuration
set /p CONFIRM="Proceed with this configuration? (y/N): "
if /i not "%CONFIRM%"=="y" (
    echo Configuration cancelled.
    pause
    exit /b 0
)

:: Configure server
echo.
echo %GREEN%[INFO]%NC% Configuring server...

:: Create JSON payload
set JSON={"start_range": "!START!", "end_range": "!END!", "pubkey": "!PUBKEY!", "dp_bits": !DP_BITS!, "range_size": "!RANGE_SIZE!"}

:: Send configuration
curl -X POST http://localhost:8080/api/configure ^
  -H "Content-Type: application/json" ^
  -d "!JSON!" ^
  -o config_response.tmp

if %errorLevel% neq 0 (
    echo %RED%[ERROR]%NC% Failed to configure server!
    echo Please check if the server is running and try again.
    del config_response.tmp 2>nul
    pause
    exit /b 1
)

:: Check response
type config_response.tmp | findstr "success" >nul
if %errorLevel% neq 0 (
    echo %RED%[ERROR]%NC% Configuration failed!
    echo Server response:
    type config_response.tmp
    del config_response.tmp
    pause
    exit /b 1
)

del config_response.tmp

echo %GREEN%[SUCCESS]%NC% Server configured successfully for Bitcoin Puzzle #%PUZZLE%!
echo.

:: Show next steps
echo %BLUE%[NEXT STEPS]%NC%
echo 1. Server is now ready to accept clients
echo 2. Build and run clients on GPU machines:
echo    - Windows: Use build_windows.bat in client/ folder
echo    - Linux: Use make in client/ folder
echo.
echo 3. Start clients with server mode:
echo    RCKangarooClient.exe -s http://YOUR_SERVER_IP:8080
echo.
echo 4. Monitor progress:
echo    - Web interface: http://localhost:8080
echo    - Status API: curl http://localhost:8080/api/status
echo.
echo %YELLOW%[IMPORTANT]%NC%
echo - Make sure firewall allows port 8080 for client connections
echo - Consider running server on a machine with good network connectivity
echo - Backup the database file regularly: kangaroo_server.db
echo.

:: Check server status
echo %GREEN%[INFO]%NC% Current server status:
curl -s http://localhost:8080/api/status | python -m json.tool 2>nul || echo Failed to get status

echo.
echo Configuration completed! Press any key to exit...
pause >nul