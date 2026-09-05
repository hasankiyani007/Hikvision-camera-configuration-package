@echo off
REM Simplified build script for HikVision Camera Setup Tool (No SDK Dependencies)

echo ========================================
echo   Building SIMPLIFIED Camera Setup Tool
echo   Using UDP Multicast Discovery Only
echo ========================================
echo.

echo [STEP 1/4] Checking required files...
if not exist "SimplifiedCameraSetup.cpp" (
    echo ERROR: SimplifiedCameraSetup.cpp not found!
    pause
    exit /b 1
)
if not exist "shared\CameraSharedLib_Simplified.cpp" (
    echo ERROR: shared\CameraSharedLib_Simplified.cpp not found!
    pause
    exit /b 1
)
if not exist "shared\NetworkManager.cpp" (
    echo ERROR: shared\NetworkManager.cpp not found!
    pause
    exit /b 1
)
if not exist "shared\SimpleSoT.cpp" (
    echo ERROR: shared\SimpleSoT.cpp not found!
    pause
    exit /b 1
)
if not exist "shared\MediaMTXConfigManager_Simplified.cpp" (
    echo ERROR: shared\MediaMTXConfigManager_Simplified.cpp not found!
    pause
    exit /b 1
)
if not exist "shared\jsoncpp.cpp" (
    echo ERROR: shared\jsoncpp.cpp not found!
    pause
    exit /b 1
)
if not exist "jsoncpp\include\json\json.h" (
    echo ERROR: JsonCpp library not found in jsoncpp\include\json\!
    echo Please ensure JsonCpp is extracted to jsoncpp\ directory
    pause
    exit /b 1
)
echo   - All required files found (no SDK dependencies)
echo.

echo [STEP 2/4] Checking compiler...
g++ --version >nul 2>&1
if %errorlevel% neq 0 (
    echo ERROR: g++ compiler not found in PATH!
    echo Make sure MinGW-w64 is installed and in PATH
    pause
    exit /b 1
)
echo   - Compiler found and ready
echo.

echo [STEP 3/4] Starting compilation (no SDK, pure UDP multicast)...
echo Include Path: .\jsoncpp\include
echo Source Files: SimplifiedCameraSetup.cpp + simplified shared modules
echo Libraries: Standard Windows networking libraries only
echo.
echo Command: g++ -std=c++17 -I".\jsoncpp\include" SimplifiedCameraSetup.cpp shared\CameraSharedLib_Simplified.cpp shared\NetworkManager.cpp shared\SimpleSoT.cpp shared\MediaMTXConfigManager_Simplified.cpp shared\jsoncpp.cpp -ladvapi32 -lws2_32 -liphlpapi -o bin\SimplifiedCameraSetup.exe
echo.
echo Compiling simplified camera setup tool...

REM Create bin directory if it doesn't exist
if not exist "bin" mkdir bin

REM Compile resource file for admin manifest
windres admin.rc admin.o

REM Compile without SDK dependencies - capture error level immediately
g++ -std=c++17 -I".\jsoncpp\include" SimplifiedCameraSetup.cpp shared\CameraSharedLib_Simplified.cpp shared\NetworkManager.cpp shared\SimpleSoT.cpp shared\MediaMTXConfigManager_Simplified.cpp shared\jsoncpp.cpp admin.o -ladvapi32 -lws2_32 -liphlpapi -o bin\SimplifiedCameraSetup.exe

set COMPILE_RESULT=%errorlevel%

echo.
echo [STEP 4/4] Checking compilation result...
if %COMPILE_RESULT% equ 0 (
    echo   - Compilation completed successfully
    echo.
    echo Verifying output file...
    REM Small delay to ensure file system sync
    timeout /t 1 /nobreak >nul 2>&1
    if exist "bin\SimplifiedCameraSetup.exe" (
        echo   - Executable created successfully
        for %%I in ("bin\SimplifiedCameraSetup.exe") do echo   - File size: %%~zI bytes
        echo.
        echo ========================================
        echo   BUILD SUCCESSFUL!
        echo ========================================
        echo.
        echo Created: bin\SimplifiedCameraSetup.exe
        echo.
        echo SIMPLIFIED DEPLOYMENT INFORMATION:
        echo ================================
        echo This executable uses NO SDK dependencies:
        echo   - Pure UDP multicast discovery (port 37020)
        echo   - Standard Windows networking libraries only
        echo   - Works with HiTools pre-configured cameras
        echo.
        echo RUNTIME REQUIREMENTS:
        echo   - No additional DLLs required
        echo   - Cameras must be pre-configured with HiTools
        echo   - All cameras must use same password
        echo.
        echo WORKFLOW:
        echo   1. Configure cameras with HiTools first
        echo   2. Run: bin\SimplifiedCameraSetup.exe
        echo   3. Enter universal password when prompted
        echo   4. Tool will discover and generate MediaMTX config
        echo.
        goto build_success
    ) else (
        echo   - ERROR: Executable not created despite successful compilation!
        echo.
        echo ========================================
        echo   BUILD FAILED!
        echo ========================================
        goto build_failed
    )
) else (
    echo   - Compilation failed with error code: %COMPILE_RESULT%
    echo.
    echo ========================================
    echo   BUILD FAILED!
    echo ========================================
    echo.
    echo Check the compilation errors above for details
    echo Common issues:
    echo   - Missing source files
    echo   - JsonCpp library not found
    echo   - Compiler path issues
    goto build_failed
)

:build_success
pause
exit /b 0

:build_failed
pause
exit /b 1