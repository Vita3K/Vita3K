@echo off
color 6
echo ============================================================
echo ====================== Vita3K Updater ======================
echo ============================================================
echo Checking for Vita3K updates...
if exist Vita3k.exe (
    for /f "delims=" %%a in ('powershell -Command "((Get-Item .\Vita3k.exe).VersionInfo.FileVersion) -replace "\"0.1.5."\""') do @set version=%%a
)
for /f "delims=" %%a in ('powershell -Command "((Invoke-WebRequest https://github.com/Vita3K/Vita3K/releases/tag/continuous).ParsedHtml.body.outerText.Split("\"`n"\") | Select-String -Pattern 'Vita3K Build:') -replace  "\"Vita3K Build: "\""') do @set git_version=%%a
if "%version%" EQU "%git_version%" (
    echo Your current version of Vita3k %version% is up-to-date, enjoy!
) else (
    if exist Vita3k.exe (
        if "%version%" NEQ "" (
               echo Your current version of Vita3k %version% is outdated!
        ) else (
               echo Your current version of Vita3k is unknown!        
        )
    ) else (
        Setlocal EnableDelayedExpansion
        echo Vita3k is not installed, do you want to install it?
        choice /c YN /n /m "Press Y for Yes, N for No."
        if !errorlevel! EQU 2 (
            echo Installation canceled!
            pause
            exit
        )
    )
    echo Attempting to download and extract the latest Vita3K version %git_version% in progress...
    powershell -Command "Invoke-WebRequest https://github.com/Vita3K/Vita3K/releases/download/continuous/windows-latest.zip -OutFile windows-latest.zip"
    if exist windows-latest.zip (
        if exist Vita3k.exe (
            taskkill /F /IM Vita3k.exe
        )
        echo Download completed, extraction in progress...
        powershell -Command "Expand-Archive -Force -Path 'windows-latest.zip' -DestinationPath '.'"
        del windows-latest.zip
        if "%version%" NEQ "" (
            echo Successfully updated your Vita3K version from %version% to %git_version%!
        ) else (
            echo Vita3K installed with success on version %git_version%!
        )
    ) else (
       echo Download failed, please try again by running the script as administrator!
    )
)
pause
