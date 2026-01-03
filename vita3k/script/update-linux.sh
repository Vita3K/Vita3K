#!/bin/sh
echo ============================================================
echo ====================== Vita3K Updater ======================
echo ============================================================

# cd to the correct path so the update doesn't happen outside the vita3k dir
cd $(dirname $(readlink -f "$0"))

boot=0

# determine architecture for correct zip
arch="$(uname -m)"
case "$arch" in
    x86_64) zip_name="ubuntu-latest.zip" ;;
    aarch64|arm64) zip_name="ubuntu-aarch64-latest.zip" ;;
    *) zip_name="vita3k-latest.zip" ;;
esac

# Download if not present
if [ ! -e "$zip_name" ]; then
    echo Checking for Vita3K updates...
    echo Attempting to download and extract the latest Vita3K version in progress...
    curl -L "https://github.com/Vita3K/Vita3K/releases/download/continuous/$zip_name" -o "./$zip_name"
else
    boot=1
fi

echo Installing update ...
unzip -q -o "./$zip_name"

# Restore execution permissions
chmod +x ./Vita3K
chmod +x ./update-vita3k.sh

# Remove the zip after extraction
rm -f "./$zip_name"

echo Vita3K updated successfully!

# Start or prompt
if [ $boot -eq 1 ]; then
    echo Starting Vita3K...
    ./Vita3K
else
    read -p "Press [Enter] key to continue..."
fi
