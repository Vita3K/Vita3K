#!/bin/sh

echo ============================================================
echo ====================== Vita3K Updater ======================
echo ============================================================

# cd to the correct path so the update doesn't happen outside the vita3k dir
cd "$(dirname "$(readlink -f "$0")")"

# Detect the system architecture
ARCH=$(uname -m)

# Set the .zip file name based on the architecture
if [ "$ARCH" = "x86_64" ]; then
    ZIP_FILE="ubuntu-x86-64-latest.zip"
elif [ "$ARCH" = "aarch64" ]; then
    ZIP_FILE="ubuntu-aarch64-latest.zip"
else
    echo "Unsupported architecture: $ARCH"
    exit 1
fi

boot=0
if [ ! -e "$ZIP_FILE" ]; then
    echo "Checking for Vita3K updates..."
    echo "Attempting to download and extract the latest Vita3K version in progress..."
    curl -L "https://github.com/Vita3K/Vita3K/releases/download/continuous/$ZIP_FILE" -o "./$ZIP_FILE"
else
    boot=1
fi

echo "Installing update ..."
unzip -q -o "./$ZIP_FILE"
rm -f "./$ZIP_FILE"
chmod +x ./Vita3K
echo "Vita3K updated with success"

if [ "$boot" -eq 1 ]; then
    echo "Starting Vita3K..."
    ./Vita3K
else
    read -p "Press [Enter] key to continue..."
fi
