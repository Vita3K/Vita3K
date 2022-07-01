#!/bin/sh
echo ============================================================
echo ====================== Vita3K Updater ======================
echo ============================================================

boot=0
if [ ! -e vita3k-latest.zip ]; then
    echo Checking for Vita3K updates...
    echo Attempting to download and extract the latest Vita3K version in progress...
    curl -L https://github.com/Vita3K/Vita3K/releases/download/continuous/ubuntu-latest.zip -o ./vita3k-latest.zip
else
    boot=1
fi

echo Installing update ...
unzip -q -o ./vita3k-latest.zip
rm -f ./vita3k-latest.zip
chmod +x ./Vita3K
echo Vita3K updated with success
if [ $boot -eq 1 ]; then
    echo start Vita3K
    ./Vita3K
else
    read -p "Press [Enter] key to continue..."
fi
