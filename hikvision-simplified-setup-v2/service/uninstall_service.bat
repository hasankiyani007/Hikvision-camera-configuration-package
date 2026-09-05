@echo off
REM ===========================================================================
REM MediaMTX Service - Uninstallation Script
REM ===========================================================================

echo ========================================
echo MediaMTX Service Uninstaller
echo ========================================
echo.

REM === Check Admin ===
net session >nul 2>&1
if %errorlevel% neq 0 (
    echo ERROR: Administrator privileges required
    pause
    exit /b 1
)

REM === Stop Service ===
echo [1/3] Stopping service...
sc query ObsidianStreamingService >nul 2>&1
if %errorlevel% equ 0 (
    sc stop ObsidianStreamingService
    timeout /t 5 /nobreak >nul
    echo OK: Service stopped
) else (
    echo OK: Service not found
)

REM === Remove Service ===
echo [2/3] Removing service...
sc query ObsidianStreamingService >nul 2>&1
if %errorlevel% equ 0 (
    sc delete ObsidianStreamingService
    timeout /t 2 /nobreak >nul
    echo OK: Service removed
)

REM === Remove Firewall Rules ===
echo [3/3] Removing firewall rules...
netsh advfirewall firewall delete rule name="MediaMTX RTSP Input"
netsh advfirewall firewall delete rule name="MediaMTX SRT Input"
netsh advfirewall firewall delete rule name="MediaMTX HLS"
netsh advfirewall firewall delete rule name="MediaMTX WebRTC HTTP"
netsh advfirewall firewall delete rule name="MediaMTX WebRTC UDP"
echo OK: Firewall rules removed

echo.
echo ========================================
echo Uninstallation Complete!
echo ========================================
echo.
echo Service has been removed.
echo.
echo Files remain at: %~dp0
echo To completely remove, manually delete this directory.
echo.
pause
