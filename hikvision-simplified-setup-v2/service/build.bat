@echo off
REM ===========================================================================
REM MediaMTX Service - Build Script (Auto-detects Visual Studio)
REM ===========================================================================

echo Building MediaMTX Service...
echo.

REM Check if already in VS environment
where cl >nul 2>&1
if %errorlevel% equ 0 (
    echo Found compiler in PATH
    goto :build
)

REM Auto-detect and initialize Visual Studio environment
echo Searching for Visual Studio installation...

REM Try Visual Studio 2022 (Community, Professional, Enterprise)
if exist "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" (
    echo Found Visual Studio 2022 Community
    call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" x64
    goto :build
)

if exist "C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvarsall.bat" (
    echo Found Visual Studio 2022 Professional
    call "C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvarsall.bat" x64
    goto :build
)

if exist "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvarsall.bat" (
    echo Found Visual Studio 2022 Enterprise
    call "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvarsall.bat" x64
    goto :build
)

REM Try Visual Studio 2019
if exist "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvarsall.bat" (
    echo Found Visual Studio 2019 Community
    call "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvarsall.bat" x64
    goto :build
)

if exist "C:\Program Files (x86)\Microsoft Visual Studio\2019\Professional\VC\Auxiliary\Build\vcvarsall.bat" (
    echo Found Visual Studio 2019 Professional
    call "C:\Program Files (x86)\Microsoft Visual Studio\2019\Professional\VC\Auxiliary\Build\vcvarsall.bat" x64
    goto :build
)

if exist "C:\Program Files (x86)\Microsoft Visual Studio\2019\Enterprise\VC\Auxiliary\Build\vcvarsall.bat" (
    echo Found Visual Studio 2019 Enterprise
    call "C:\Program Files (x86)\Microsoft Visual Studio\2019\Enterprise\VC\Auxiliary\Build\vcvarsall.bat" x64
    goto :build
)

REM Try Visual Studio Build Tools 2022
if exist "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvarsall.bat" (
    echo Found Visual Studio Build Tools 2022
    call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvarsall.bat" x64
    goto :build
)

REM Try Visual Studio Build Tools 2019
if exist "C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\VC\Auxiliary\Build\vcvarsall.bat" (
    echo Found Visual Studio Build Tools 2019
    call "C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\VC\Auxiliary\Build\vcvarsall.bat" x64
    goto :build
)

REM Not found
echo.
echo ERROR: Visual Studio not found
echo.
echo Please install one of:
echo - Visual Studio 2022 (Community/Professional/Enterprise)
echo - Visual Studio 2019 (Community/Professional/Enterprise)
echo - Visual Studio Build Tools 2022/2019
echo.
echo Download: https://visualstudio.microsoft.com/downloads/
echo.
pause
exit /b 1

:build
echo.

REM Clean old build
if exist MediaMTXService.exe del MediaMTXService.exe
if exist MediaMTXService.obj del MediaMTXService.obj

REM Compile with proper flags
echo Compiling MediaMTXService.cpp...
cl /EHsc /O2 /W3 /nologo ^
    /D_CRT_SECURE_NO_WARNINGS ^
    /I..\jsoncpp\include ^
    MediaMTXService.cpp ^
    ..\shared\jsoncpp.cpp ^
    /link ^
    advapi32.lib ^
    winhttp.lib ^
    /OUT:MediaMTXService.exe

if %errorlevel% neq 0 (
    echo.
    echo ERROR: Compilation failed
    pause
    exit /b 1
)

REM Cleanup obj files
if exist MediaMTXService.obj del MediaMTXService.obj
if exist jsoncpp.obj del jsoncpp.obj

echo.
echo ========================================
echo Build Successful!
echo ========================================
echo.
echo Output: MediaMTXService.exe
echo Size:
dir MediaMTXService.exe | find "MediaMTXService.exe"
echo.
pause
