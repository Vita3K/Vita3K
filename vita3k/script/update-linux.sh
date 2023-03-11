#!/bin/sh
header="============================================================"
echo "$header"
echo ====================== Vita3K Updater ======================
echo "$header"
# cd to the correct path so the update doesn't happen outside the vita3k dir
cd $(dirname $(readlink -f "$0"))

boot=0
[ ! -e vita3k-latest.zip ] && {
    echo Checking for Vita3K updates...
    echo Attempting to download and extract the latest Vita3K version in progress...
    curl -L https://github.com/Vita3K/Vita3K/releases/download/continuous/ubuntu-latest.zip -o ./vita3k-latest.zip
} || boot=1

echo Installing update ...
unzip -q -o ./vita3k-latest.zip
rm -f ./vita3k-latest.zip
chmod +x ./Vita3K
echo Vita3K updated with success
[ "$boot" -eq 1 ] && {
    echo start Vita3K
    ./Vita3K
} || read -p "Press [Enter] key to continue..."
