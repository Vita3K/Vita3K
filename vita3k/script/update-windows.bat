@echo off
color 2
title Vita3K Updater
timeout /t 1 /nobreak >nul

echo ============================================================
echo ====================== Vita3K Updater ======================
echo ============================================================

for /f "delims=" %%g in ('powershell "((Invoke-RestMethod https://api.github.com/repos/Vita3K/Vita3K/releases/latest -timeout 2).body.Split("\"`n"\") | Select-String -Pattern 'Vita3K Build:') -replace  'Vita3K Build: '"') do @set git_version=%%g
if exist Vita3K.exe (
    for /f "delims=" %%v in ('powershell "(((Get-Item Vita3K.exe).VersionInfo.FileVersion) -split '\.')[3]"') do @set version=%%v
)
set boot=0

if not exist vita3k-latest.zip (
    echo Checking for Vita3K updates...
    if "%version%" EQU "%git_version%" (
        echo Your current version of Vita3K %version% is up-to-date, enjoy!
        pause
        exit
    )
    if exist Vita3K.exe (
        if "%version%" NEQ "" (
            echo Your current version of Vita3K %version% is outdated!
        ) else (
            echo Your current version of Vita3K is unknown.
        )
    ) else (
        setlocal enableDelayedExpansion
        echo Vita3K is not installed, do you want to install it?
        choice /c YN /n /m "Press Y for Yes, N for No."
        if !errorlevel! EQU 2 (
            echo Installation canceled.
            pause
            exit
        )
        endlocal
    )
    setlocal enableDelayedExpansion
    set arch=%PROCESSOR_ARCHITECTURE%
    if "!arch!" == "AMD64" (
        set zip_name=windows-latest.zip
    ) else if "!arch!" == "ARM64" (
        set zip_name=windows-arm64-latest.zip
    ) else (
        echo Unsupported architecture: !arch!
        pause
        exit
    )
    echo Attempting to download and extract the latest Vita3K version %git_version% in progress...
    powershell "Invoke-WebRequest https://github.com/Vita3K/Vita3K/releases/download/continuous/!zip_name! -OutFile vita3k-latest.zip"
    endlocal
    if exist Vita3K.exe (
       taskkill /F /IM Vita3K.exe
    )
) else (
    set boot=1
)

if exist vita3k-latest.zip (
    echo Download completed, extraction in progress...
    powershell "Expand-Archive -Force -Path vita3k-latest.zip -DestinationPath '.'"
    del vita3k-latest.zip
    if "%version%" NEQ "" (
        echo Successfully updated your Vita3K version from %version% to %git_version%!
    ) else (
        echo Vita3K installed with success on version %git_version%!
    )
    if %boot% EQU 1 (
        echo Starting Vita3K...
        Vita3K.exe
    ) else (
        echo You can start Vita3K by running Vita3K.exe
    )
) else if %boot% EQU 0 (
    echo Download failed, please try again by running the script as administrator.
)

if %boot% EQU 0 (
    pause
)
