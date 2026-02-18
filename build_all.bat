@echo off
setlocal enabledelayedexpansion

echo ===========================================
echo  AstroSpike Build Script (MinGW + Qt6 + Ninja)
echo ===========================================

set "SILENT_MODE=0"
if "%1"=="--silent" set "SILENT_MODE=1"

REM Move to project root
pushd "%~dp0"
set PROJECT_ROOT=%CD%

REM --- AUTO-DETECT TOOLS ---
call :FindMinGW
if "!MINGW_BIN!"=="" (
    echo [ERROR] MinGW not found. Please install Qt Creator with MinGW.
    goto :error
)
echo [INFO] MinGW found: !MINGW_BIN!

call :FindQtPath
if "!QT_PATH!"=="" (
    echo [ERROR] Qt6 not found. Please install Qt6.
    goto :error
)
echo [INFO] Qt6 found: !QT_PATH!

call :FindNinja
if "!NINJA_CMD!"=="" (
    echo [WARNING] Ninja not found. Will use MinGW Makefiles.
    set CMAKE_GENERATOR=MinGW Makefiles
    set NINJA_FOUND=0
) else (
    echo [INFO] Ninja found: !NINJA_CMD!
    set CMAKE_GENERATOR=Ninja
    set NINJA_FOUND=1
)

REM Verify CMake
for %%X in (cmake.exe) do set "CMAKE_PATH=%%~$PATH:X"
if "!CMAKE_PATH!"=="" (
    echo [ERROR] CMake not found in PATH!
    goto :error
)
echo [INFO] CMake found: !CMAKE_PATH!

REM Verify g++
if not exist "!MINGW_BIN!\g++.exe" (
    echo [ERROR] g++ not found at !MINGW_BIN!\g++.exe
    goto :error
)

REM --- SETUP BUILD ENVIRONMENT ---
set "PATH=!MINGW_BIN!;!PATH!"

REM --- CLEAN BUILD ---
if exist build (
    echo Removing old build directory...
    rmdir /s /q build
)

mkdir build
cd build

REM --- CMAKE CONFIGURATION ---
echo.
echo Configuring CMake with !CMAKE_GENERATOR!...
cmake -G "!CMAKE_GENERATOR!" ^
    -DCMAKE_PREFIX_PATH="!QT_PATH!" ^
    -DCMAKE_BUILD_TYPE=Release ^
    ..

if !ERRORLEVEL! neq 0 (
    echo [ERROR] CMake configuration failed!
    cd ..
    goto :error
)

REM --- BUILD ---
echo.
echo Building AstroSpike...
if !NINJA_FOUND!==1 (
    !NINJA_CMD! -j 8
) else (
    cmake --build . --config Release -j 8
)

if !ERRORLEVEL! neq 0 (
    echo [ERROR] Build failed!
    cd ..
    goto :error
)

cd ..

echo.
echo Deploying dependencies...
call deploy.bat
if !ERRORLEVEL! neq 0 (
    echo [ERROR] Deployment failed!
    goto :error
)

echo.
echo ==========================================
echo  Build and Deployment Complete!
echo  Executable: %PROJECT_ROOT%\build\AstroSpike.exe
echo ==========================================
if !SILENT_MODE!==0 pause
exit /b 0

:error
echo.
echo ==========================================
echo  Build FAILED
echo ==========================================
if !SILENT_MODE!==0 pause
exit /b 1

REM ============================================================================
REM UTILITY FUNCTIONS
REM ============================================================================

:FindMinGW
setlocal enabledelayedexpansion
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
endlocal & set MINGW_BIN=%MINGW_BIN%
exit /b 0

:FindQtPath
setlocal enabledelayedexpansion
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
endlocal & set QT_PATH=%QT_PATH%
exit /b 0

:FindNinja
setlocal enabledelayedexpansion
set "NINJA_CMD="
if exist "C:\Tools\ninja\ninja.exe" (
    set "NINJA_CMD=C:\Tools\ninja\ninja.exe"
) else (
    for %%X in (ninja.exe) do (
        set "NINJA_PATH=%%~$PATH:X"
        if not "!NINJA_PATH!"=="" (
            set "NINJA_CMD=!NINJA_PATH!"
        )
    )
)
endlocal & set NINJA_CMD=%NINJA_CMD%
exit /b 0
