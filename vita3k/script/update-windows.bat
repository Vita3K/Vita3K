@echo Download and extract the latest build of Vita3K in progress...
taskkill /F /IM Vita3k.exe
powershell -Command "Invoke-WebRequest https://github.com/Vita3K/Vita3K/releases/download/continuous/windows-latest.zip -OutFile windows-latest.zip"
powershell -Command "Expand-Archive -Force -Path 'windows-latest.zip' -DestinationPath '.'"
del windows-latest.zip
@echo Finished!
pause
