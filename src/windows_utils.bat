@echo off
REM =============================================================================
REM AstroSpike Windows Build Utilities
REM Shared functions for build_all.bat
REM =============================================================================

REM --- Tool Detection ---
REM Call with: call :FindMinGW
:FindMinGW
setlocal enabledelayedexpansion
set "MINGW_BIN="
if exist "C:\Qt\Tools\mingw1310_64\bin" (
    set "MINGW_BIN=C:\Qt\Tools\mingw1310_64\bin"
    endlocal & set MINGW_BIN=%MINGW_BIN%
    exit /b 0
)
if exist "C:\Qt\Tools\mingw1220_64\bin" (
    set "MINGW_BIN=C:\Qt\Tools\mingw1220_64\bin"
    endlocal & set MINGW_BIN=%MINGW_BIN%
    exit /b 0
)
if exist "C:\Qt\Tools\mingw\bin" (
    set "MINGW_BIN=C:\Qt\Tools\mingw\bin"
    endlocal & set MINGW_BIN=%MINGW_BIN%
    exit /b 0
)
if exist "C:\msys64\mingw64\bin" (
    set "MINGW_BIN=C:\msys64\mingw64\bin"
    endlocal & set MINGW_BIN=%MINGW_BIN%
    exit /b 0
)
REM Fallback to PATH search
for %%X in (g++.exe) do (
    set "GXX_PATH=%%~$PATH:X"
    if not "!GXX_PATH!"=="" (
        for %%A in ("!GXX_PATH!\.") do set "MINGW_BIN=%%~fA"
        endlocal & set MINGW_BIN=!MINGW_BIN!
        exit /b 0
    )
)
endlocal
exit /b 1

REM Call with: call :FindQtPath
:FindQtPath
setlocal enabledelayedexpansion
set "QT_PATH="
if exist "C:\Qt\6.10.1\mingw_64" (
    set "QT_PATH=C:\Qt\6.10.1\mingw_64"
    endlocal & set QT_PATH=%QT_PATH%
    exit /b 0
)
if exist "C:\Qt\6.9.2\mingw_64" (
    set "QT_PATH=C:\Qt\6.9.2\mingw_64"
    endlocal & set QT_PATH=%QT_PATH%
    exit /b 0
)
if exist "C:\Qt\6.8.1\mingw_64" (
    set "QT_PATH=C:\Qt\6.8.1\mingw_64"
    endlocal & set QT_PATH=%QT_PATH%
    exit /b 0
)
if exist "C:\Qt\6.7.0\mingw_64" (
    set "QT_PATH=C:\Qt\6.7.0\mingw_64"
    endlocal & set QT_PATH=%QT_PATH%
    exit /b 0
)
for %%X in (qmake.exe) do (
    set "QMAKE_PATH=%%~$PATH:X"
    if not "!QMAKE_PATH!"=="" (
        for %%A in ("!QMAKE_PATH!\..\..") do set "QT_PATH=%%~fA"
        endlocal & set QT_PATH=!QT_PATH!
        exit /b 0
    )
)
endlocal
exit /b 1

REM Call with: call :FindNinja
:FindNinja
setlocal enabledelayedexpansion
set "NINJA_CMD="
if exist "C:\Tools\ninja\ninja.exe" (
    set "NINJA_CMD=C:\Tools\ninja\ninja.exe"
    endlocal & set NINJA_CMD=%NINJA_CMD%
    exit /b 0
)
for %%X in (ninja.exe) do (
    set "NINJA_PATH=%%~$PATH:X"
    if not "!NINJA_PATH!"=="" (
        set "NINJA_CMD=!NINJA_PATH!"
        endlocal & set NINJA_CMD=!NINJA_CMD!
        exit /b 0
    )
)
endlocal
exit /b 1
