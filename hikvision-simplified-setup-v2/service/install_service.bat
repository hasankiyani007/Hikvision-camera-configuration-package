@echo off
REM ===========================================================================
REM MediaMTX Service - Flexible Installation Script
REM ===========================================================================
REM
REM USAGE:
REM   1. Extract zip anywhere (e.g., C:\HikVision\)
REM   2. Run camera setup tool to configure mediamtx.yml
REM   3. Run this script as Administrator
REM
REM The service will run from the CURRENT directory (no move to Program Files)
REM ===========================================================================

SET CURRENT_DIR=%~dp0
SET CURRENT_DIR=%CURRENT_DIR:~0,-1%

echo ========================================
echo MediaMTX Service Installation
echo ========================================
echo.
echo Installation directory: %CURRENT_DIR%
echo.

REM === Check Admin ===
net session >nul 2>&1
if %errorlevel% neq 0 (
    echo ERROR: Administrator privileges required
    echo Right-click and select "Run as Administrator"
    echo.
    pause
    exit /b 1
)

REM === Verify Files Exist ===
echo [1/5] Verifying files...

if not exist "%CURRENT_DIR%\MediaMTXService.exe" (
    echo ERROR: MediaMTXService.exe not found in current directory
    pause
    exit /b 1
)

if not exist "%CURRENT_DIR%\mediamtx.exe" (
    echo ERROR: mediamtx.exe not found
    pause
    exit /b 1
)

if not exist "%CURRENT_DIR%\ffmpeg.exe" (
    echo ERROR: ffmpeg.exe not found
    pause
    exit /b 1
)

if not exist "%CURRENT_DIR%\mediamtx.yml" (
    echo ERROR: mediamtx.yml not found
    echo.
    echo Please run the camera setup tool first to generate configuration.
    pause
    exit /b 1
)

echo OK: All required files present

REM === Stop Existing Service ===
echo [2/5] Checking for existing service...

sc query ObsidianStreamingService >nul 2>&1
if %errorlevel% equ 0 (
    echo Found existing service, stopping...
    sc stop ObsidianStreamingService
    timeout /t 5 /nobreak >nul

    echo Uninstalling old service...
    sc delete ObsidianStreamingService
    timeout /t 2 /nobreak >nul
)

echo OK: Ready for installation

REM === Configure Firewall ===
echo [3/5] Configuring Windows Firewall...

echo Removing old rules...
netsh advfirewall firewall delete rule name="MediaMTX RTSP Input" >nul 2>&1
netsh advfirewall firewall delete rule name="MediaMTX SRT Input" >nul 2>&1
netsh advfirewall firewall delete rule name="MediaMTX HLS" >nul 2>&1
netsh advfirewall firewall delete rule name="MediaMTX WebRTC HTTP" >nul 2>&1
netsh advfirewall firewall delete rule name="MediaMTX WebRTC UDP" >nul 2>&1

echo Adding required rules...
netsh advfirewall firewall add rule ^
    name="MediaMTX RTSP Input" ^
    dir=in action=allow protocol=TCP localport=8554 ^
    description="Cameras send RTSP streams to MediaMTX" || goto :error

echo Adding optional rules (for client access)...
netsh advfirewall firewall add rule ^
    name="MediaMTX HLS" ^
    dir=in action=allow protocol=TCP localport=8888 ^
    description="Browser HLS stream viewing" >nul 2>&1

netsh advfirewall firewall add rule ^
    name="MediaMTX WebRTC HTTP" ^
    dir=in action=allow protocol=TCP localport=8889 ^
    description="WebRTC signaling" >nul 2>&1

netsh advfirewall firewall add rule ^
    name="MediaMTX WebRTC UDP" ^
    dir=in action=allow protocol=UDP localport=8189 ^
    description="WebRTC media" >nul 2>&1

echo OK: Firewall configured

REM === Install Service ===
echo [4/5] Installing Windows service...

"%CURRENT_DIR%\MediaMTXService.exe" --install || goto :error

echo OK: Service registered

REM === Start Service ===
echo [5/5] Starting service...

sc start ObsidianStreamingService || goto :error

timeout /t 3 /nobreak >nul

REM === Verify Running ===
sc query ObsidianStreamingService | find "RUNNING" >nul
if %errorlevel% neq 0 (
    echo WARNING: Service installed but not running
    echo.
    sc query ObsidianStreamingService
    echo.
    echo Check logs: %CURRENT_DIR%\MediaMTXService.log
    pause
    exit /b 1
)

echo OK: Service running

REM === Installation Complete ===
echo.
echo ========================================
echo Installation Successful!
echo ========================================
echo.
echo Service Details:
sc query ObsidianStreamingService
echo.
echo Installation Location:
echo   %CURRENT_DIR%
echo.
echo Configuration:
echo   Auto-start: Yes (starts on boot)
echo   Recovery: Auto-restart on crash (30s/60s/120s delays)
echo   Monitoring: Every 30 seconds
echo.
echo Network Ports:
echo   TCP 8554  - Camera RTSP input (required)
echo   TCP 8888  - Browser HLS viewing (optional)
echo   TCP 8889  - WebRTC HTTP (optional)
echo   UDP 8189  - WebRTC media (optional)
echo   TCP 9997  - API (localhost only, no firewall rule)
echo.
echo Logs:
echo   %CURRENT_DIR%\MediaMTXService.log
echo.
echo API Test:
echo   http://127.0.0.1:9997/v3/paths/list
echo.
pause
exit /b 0

:error
echo.
echo ========================================
echo Installation Failed!
echo ========================================
echo.
pause
exit /b 1
