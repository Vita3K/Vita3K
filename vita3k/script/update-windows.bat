@echo off
color 2
title RPCSV Updater
timeout /t 1 /nobreak >nul

echo ============================================================
echo ====================== RPCSV Updater ======================
echo ============================================================

for /f "delims=" %%g in ('powershell "((Invoke-RestMethod https://api.github.com/repos/RPCSV/RPCSV/releases/latest -timeout 2).body.Split("\"`n"\") | Select-String -Pattern 'RPCSV Build:') -replace  'RPCSV Build: '"') do @set git_version=%%g
if exist RPCSV.exe (
   for /f "delims=" %%v in ('powershell "((Get-Item RPCSV.exe).VersionInfo.FileVersion) -replace '0.2.0.'"') do @set version=%%v
)
set boot=0

if not exist RPCSV-latest.zip (
   echo Checking for RPCSV updates...
   if "%version%" EQU "%git_version%" (
       echo Your current version of RPCSV %version% is up-to-date, enjoy!
       pause
       exit
    ) else (
        if exist RPCSV.exe (
            if "%version%" NEQ "" (
                echo Your current version of RPCSV %version% is outdated!
            ) else (
                echo Your current version of RPCSV is unknown.
            )
        ) else (
            Setlocal EnableDelayedExpansion
            echo RPCSV is not installed, do you want to install it?
            choice /c YN /n /m "Press Y for Yes, N for No."
            if !errorlevel! EQU 2 (
                echo Installation canceled.
                pause
                exit
            )
        )
        echo Attempting to download and extract the latest RPCSV version %git_version% in progress...
        powershell "Invoke-WebRequest https://github.com/RPCSV/RPCSV/releases/download/continuous/windows-latest.zip -OutFile RPCSV-latest.zip"
        if exist RPCSV.exe (
           taskkill /F /IM RPCSV.exe
        )
    )
) else (
    set boot=1
)

if exist RPCSV-latest.zip (
    echo Download completed, extraction in progress...
    powershell "Expand-Archive -Force -Path RPCSV-latest.zip -DestinationPath '.'"
    del RPCSV-latest.zip
    if "%version%" NEQ "" (
        echo Successfully updated your RPCSV version from %version% to %git_version%!
    ) else (
        echo RPCSV installed with success on version %git_version%!
    )
    if %boot% EQU 1 (
        echo Starting RPCSV...
        RPCSV.exe
    ) else (
        echo You can start RPCSV by running RPCSV.exe
    )
) else if %boot% EQU 0 (
    echo Download failed, please try again by running the script as administrator.
)

if %boot% EQU 0 (
    pause
)
