@echo off
REM ============================================================
REM Student Character Animation Plugin - Build Script
REM Run this from any directory — it finds setup.cmd automatically.
REM ============================================================

echo ========================================
echo Student Character Animation Plugin Build
echo ========================================

REM Step 1: Resolve N8RO root (4 directories above this script)
REM  This script: dev\samples\sim\student-char-anim\build.cmd
REM  N8RO root:   ..\..\..\..  = c:\N8RO
set "N8RO_ROOT=%~dp0..\..\..\..\"

REM Run setup.cmd if not already done
if not defined N8RO_SETUP_DONE (
    echo [Info] Running setup.cmd from %N8RO_ROOT%...
    call "%N8RO_ROOT%setup.cmd"
    if errorlevel 1 (
        echo [Error] setup.cmd failed.
        exit /b 1
    )
)

echo [OK] N8RO_RELEASE = %N8RO_RELEASE%
echo [OK] N8RO_RELEASE_USER_SIM_PLUGINS = %N8RO_RELEASE_USER_SIM_PLUGINS%

REM Step 2: Initialize MSVC build environment
echo.
echo [Info] Setting up Visual Studio build environment...
call "%N8RO_RELEASE%\dev\setup-dev.cmd"
if errorlevel 1 (
    echo [Error] dev\setup-dev.cmd failed.
    echo         Is Visual Studio 2022 Insiders installed?
    exit /b 1
)
echo [OK] MSBuild ready at: %N8RO_RELEASE_MSBUILD_CMD%

REM Step 3: Build the plugin
echo.
echo [Build] Building student-char-anim.dll ...

"%N8RO_RELEASE_MSBUILD_CMD%" "%N8RO_RELEASE%\dev\samples\sim\student-char-anim\student-char-anim.vcxproj" /p:Configuration=Release /p:Platform=x64 /p:N8RO_RELEASE=%N8RO_RELEASE% /p:N8RO_RELEASE_USER_SIM_PLUGINS=%N8RO_RELEASE_USER_SIM_PLUGINS% /v:minimal

if errorlevel 1 (
    echo.
    echo [FAILED] Build failed. See errors above.
    exit /b 1
)

echo.
echo ========================================
echo [SUCCESS] Build complete!
echo.
echo  DLL output:
echo    %N8RO_RELEASE%\dev\samples\sim\student-char-anim\bin\release\student-char-anim.dll
echo.
echo  Auto-deployed to:
echo    %N8RO_RELEASE_USER_SIM_PLUGINS%\student-char-anim.dll
echo.
echo  Verify exports:
echo    dumpbin /exports "%N8RO_RELEASE_USER_SIM_PLUGINS%\student-char-anim.dll"
echo ========================================
