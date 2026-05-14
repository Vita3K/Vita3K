#!/bin/sh
echo ============================================================
echo ====================== RPCSV Updater ======================
echo ============================================================
# cd to the correct path so the update doesn't happen outside the RPCSV dir
cd $(dirname $(readlink -f "$0"))

boot=0
if [ ! -e RPCSV-latest.zip ]; then
    echo Checking for RPCSV updates...
    echo Attempting to download and extract the latest RPCSV version in progress...
    curl -L https://github.com/RPCSV/RPCSV/releases/download/continuous/ubuntu-latest.zip -o ./RPCSV-latest.zip
else
    boot=1
fi

echo Installing update ...
unzip -q -o ./RPCSV-latest.zip
rm -f ./RPCSV-latest.zip
chmod +x ./RPCSV
echo RPCSV updated with success
if [ $boot -eq 1 ]; then
    echo start RPCSV
    ./RPCSV
else
    read -p "Press [Enter] key to continue..."
fi
