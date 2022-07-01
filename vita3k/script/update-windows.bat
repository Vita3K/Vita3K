@echo off
color 2
echo ============================================================
echo ====================== Vita3K Updater ======================
echo ============================================================

for /f "delims=" %%g in ('powershell "((Invoke-WebRequest https://github.com/Vita3K/Vita3K/releases/tag/continuous).ParsedHtml.body.outerText.Split("\"`n"\") | Select-String -Pattern 'Vita3K Build:') -replace  'Vita3K Build: '"') do @set git_version=%%g
if exist Vita3k.exe (
   for /f "delims=" %%v in ('powershell "((Get-Item Vita3k.exe).VersionInfo.FileVersion) -replace '0.1.5.'"') do @set version=%%v
)
set boot=0

if not exist vita3k-latest.zip (
   echo Checking for Vita3K updates...
   if "%version%" EQU "%git_version%" (
       echo Your current version of Vita3k %version% is up-to-date, enjoy!
       pause
       exit
    ) else (
        if exist Vita3k.exe (
            if "%version%" NEQ "" (
                echo Your current version of Vita3k %version% is outdated!
            ) else (
                echo Your current version of Vita3k is unknown.
            )
        ) else (
            Setlocal EnableDelayedExpansion
            echo Vita3k is not installed, do you want to install it?
            choice /c YN /n /m "Press Y for Yes, N for No."
            if !errorlevel! EQU 2 (
                echo Installation canceled.
                pause
                exit
            )
        )
        echo Attempting to download and extract the latest Vita3K version %git_version% in progress...
        powershell "Invoke-WebRequest https://github.com/Vita3K/Vita3K/releases/download/continuous/windows-latest.zip -OutFile vita3k-latest.zip"
        if exist Vita3k.exe (
           taskkill /F /IM Vita3k.exe
        )
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
        vita3k.exe
    ) else (
        echo You can start Vita3K by running vita3k.exe
    )
) else if %boot% EQU 0 (
    echo Download failed, please try again by running the script as administrator.
)
pause
