@echo off
setlocal enabledelayedexpansion

echo ===========================================
echo  AstroSpike Deployment Script
echo ===========================================

set "SILENT_MODE=0"
if "%1"=="--silent" set "SILENT_MODE=1"

REM Move to project root
pushd "%~dp0"
set PROJECT_ROOT=%CD%

REM Detect build directory
if exist "build\AstroSpike.exe" (
    set "BUILD_DIR=build"
) else (
    echo [ERROR] AstroSpike.exe not found in build directory.
    echo Please build the project first.
    if !SILENT_MODE!==0 pause
    exit /b 1
)

set "EXE_PATH=!BUILD_DIR!\AstroSpike.exe"

REM --- DETECT TOOLS (similar to build_all.bat) ---
call :FindMinGW
if "!MINGW_BIN!"=="" (
    echo [ERROR] MinGW not found.
    exit /b 1
)

call :FindQtPath
if "!QT_PATH!"=="" (
    echo [ERROR] Qt6 not found.
    exit /b 1
)

set "QT_BIN=!QT_PATH!\bin"
set "WINDEPLOYQT=!QT_BIN!\windeployqt.exe"

echo [INFO] Using build directory: !BUILD_DIR!
echo [INFO] Qt Bin: !QT_BIN!

if not exist "!WINDEPLOYQT!" (
    echo [ERROR] windeployqt.exe not found at !WINDEPLOYQT!
    exit /b 1
)

echo [STEP 1] Running windeployqt...
"!WINDEPLOYQT!" --release --compiler-runtime "!EXE_PATH!"
if !ERRORLEVEL! neq 0 (
    echo [ERROR] windeployqt failed!
    exit /b 1
)

echo [STEP 2] Copying MinGW runtime DLLs...
copy "!MINGW_BIN!\libgcc_s_seh-1.dll" "!BUILD_DIR!\" >nul 2>&1
copy "!MINGW_BIN!\libstdc++-6.dll" "!BUILD_DIR!\" >nul 2>&1
copy "!MINGW_BIN!\libwinpthread-1.dll" "!BUILD_DIR!\" >nul 2>&1
copy "!MINGW_BIN!\libgomp-1.dll" "!BUILD_DIR!\" >nul 2>&1

echo [STEP 3] Copying local dependencies...

REM Copy GSL DLLs
if exist "deps\gsl\bin\libgsl-28.dll" (
    echo   - GSL: Found
    copy "deps\gsl\bin\libgsl-28.dll" "!BUILD_DIR!\" >nul 2>&1
    copy "deps\gsl\bin\libgslcblas-0.dll" "!BUILD_DIR!\" >nul 2>&1
)

REM Copy OpenCV DLLs
echo   - OpenCV: Copying...
copy "deps\opencv\x64\mingw\bin\libopencv_core*.dll" "!BUILD_DIR!\" >nul 2>&1
copy "deps\opencv\x64\mingw\bin\libopencv_imgproc*.dll" "!BUILD_DIR!\" >nul 2>&1
copy "deps\opencv\x64\mingw\bin\libopencv_imgcodecs*.dll" "!BUILD_DIR!\" >nul 2>&1
copy "deps\opencv\x64\mingw\bin\libopencv_videoio*.dll" "!BUILD_DIR!\" >nul 2>&1
copy "deps\opencv\x64\mingw\bin\libopencv_highgui*.dll" "!BUILD_DIR!\" >nul 2>&1
copy "deps\opencv\x64\mingw\bin\libopencv_calib3d*.dll" "!BUILD_DIR!\" >nul 2>&1
copy "deps\opencv\x64\mingw\bin\libopencv_features2d*.dll" "!BUILD_DIR!\" >nul 2>&1
copy "deps\opencv\x64\mingw\bin\libopencv_flann*.dll" "!BUILD_DIR!\" >nul 2>&1
copy "deps\opencv\x64\mingw\bin\libopencv_objdetect*.dll" "!BUILD_DIR!\" >nul 2>&1
copy "deps\opencv\x64\mingw\bin\libopencv_photo*.dll" "!BUILD_DIR!\" >nul 2>&1
copy "deps\opencv\x64\mingw\bin\libopencv_stitching*.dll" "!BUILD_DIR!\" >nul 2>&1
copy "deps\opencv\x64\mingw\bin\libopencv_video*.dll" "!BUILD_DIR!\" >nul 2>&1

echo.
echo [SUCCESS] Dependencies collected in !BUILD_DIR!
echo You can now run AstroSpike.exe!
exit /b 0

REM ============================================================================
REM UTILITY FUNCTIONS (Copied from build_all.bat for consistency)
REM ============================================================================

:FindMinGW
set "MINGW_BIN="
if exist "C:\Qt\Tools\mingw1310_64\bin" (
    set "MINGW_BIN=C:\Qt\Tools\mingw1310_64\bin"
) else if exist "C:\Qt\Tools\mingw1220_64\bin" (
    set "MINGW_BIN=C:\Qt\Tools\mingw1220_64\bin"
) else if exist "C:\Qt\Tools\mingw\bin" (
    set "MINGW_BIN=C:\Qt\Tools\mingw\bin"
) else if exist "C:\msys64\mingw64\bin" (
    set "MINGW_BIN=C:\msys64\mingw64\bin"
) else (
    for %%X in (g++.exe) do (
        set "GXX_PATH=%%~$PATH:X"
        if not "!GXX_PATH!"=="" (
            for %%A in ("!GXX_PATH!\.") do set "MINGW_BIN=%%~fA"
        )
    )
)
exit /b 0

:FindQtPath
set "QT_PATH="
if exist "C:\Qt\6.10.1\mingw_64" (
    set "QT_PATH=C:\Qt\6.10.1\mingw_64"
) else if exist "C:\Qt\6.9.2\mingw_64" (
    set "QT_PATH=C:\Qt\6.9.2\mingw_64"
) else if exist "C:\Qt\6.8.1\mingw_64" (
    set "QT_PATH=C:\Qt\6.8.1\mingw_64"
) else if exist "C:\Qt\6.7.0\mingw_64" (
    set "QT_PATH=C:\Qt\6.7.0\mingw_64"
) else (
    for %%X in (qmake.exe) do (
        set "QMAKE_PATH=%%~$PATH:X"
        if not "!QMAKE_PATH!"=="" (
            for %%A in ("!QMAKE_PATH!\..\..") do set "QT_PATH=%%~fA"
        )
    )
)
exit /b 0
