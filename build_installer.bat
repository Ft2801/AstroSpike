@echo off
setlocal enabledelayedexpansion

echo ===========================================
echo  AstroSpike Installer Builder
echo ===========================================
echo.

REM Move to project root
pushd "%~dp0"
set PROJECT_ROOT=%CD%

REM --- Read version from changelog.txt ---
call :GetVersion
echo [INFO] Building version: %VERSION%
echo.

REM --- STEP 0: Verify Prerequisites ---
echo [STEP 0] Verifying prerequisites...

REM Check if Inno Setup is installed
call :FindInnoSetup
if "!ISCC!"=="" (
    echo [ERROR] Inno Setup 6 not found!
    echo Please install Inno Setup from: https://jrsoftware.org/isdl.php
    pause
    exit /b 1
)
echo   - Inno Setup 6: OK at !ISCC!

if not exist "src\installer.iss" (
    echo [ERROR] src\installer.iss not found!
    pause
    exit /b 1
)
echo   - src\installer.iss: OK
echo.

REM --- STEP 1: Clean Previous Installer Output ---
echo [STEP 1] Cleaning previous installer output...
if exist "installer_output" rmdir /s /q "installer_output"
mkdir "installer_output"
echo.

REM --- STEP 2: Build and Deploy the Application ---
echo [STEP 2] Building and deploying application...
call build_all.bat --silent
if !ERRORLEVEL! neq 0 (
    echo [ERROR] Build flow failed!
    pause
    exit /b 1
)
echo   - Build and Deploy: OK
echo.

REM --- STEP 3: Create Installer ---
echo [STEP 3] Creating installer with Inno Setup...
"!ISCC!" /DMyAppVersion="%VERSION%" src\installer.iss
if !ERRORLEVEL! neq 0 (
    echo [ERROR] Installer creation failed!
    pause
    exit /b 1
)
echo.

echo ===========================================
echo  SUCCESS! Installer Build Complete
echo ===========================================
pause
exit /b 0

:FindInnoSetup
set "ISCC="
if exist "C:\Program Files (x86)\Inno Setup 6\ISCC.exe" (
    set "ISCC=C:\Program Files (x86)\Inno Setup 6\ISCC.exe"
    exit /b 0
)
if exist "C:\Program Files\Inno Setup 6\ISCC.exe" (
    set "ISCC=C:\Program Files\Inno Setup 6\ISCC.exe"
    exit /b 0
)
for /f "tokens=2*" %%A in ('reg query "HKLM\Software\Microsoft\Windows\CurrentVersion\Uninstall\Inno Setup 6_is1" /v "InstallLocation" 2^>nul') do (
    if exist "%%B\ISCC.exe" (
        set "ISCC=%%B\ISCC.exe"
        exit /b 0
    )
)
exit /b 0

:GetVersion
set "VERSION=1.0.0"
if exist "changelog.txt" (
    for /f "tokens=2" %%v in ('type "changelog.txt" ^| findstr /R "^Version [0-9]"') do (
        set "VERSION=%%v"
        goto :EOF
    )
)
goto :EOF
