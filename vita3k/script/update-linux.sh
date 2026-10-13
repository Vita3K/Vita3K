#!/bin/bash
echo ============================================================
echo ====================== Vita3K Updater ======================
echo ============================================================

# cd to the correct path so the update doesn't happen outside the vita3k dir
cd -- "$(dirname -- "$(readlink -f -- "$0")")" || exit 1

boot=0

# determine architecture for correct zip
arch="$(uname -m)"
case "$arch" in
    x86_64|amd64) zip_name="ubuntu-latest.zip" ;;
    aarch64|arm64) zip_name="ubuntu-aarch64-latest.zip" ;;
    *)
      echo "Unsupported architecture: $arch"
      exit 1
      ;;
esac

# Download if not present
if [ ! -e "vita3k-latest.zip" ]; then
    echo Checking for Vita3K updates...
    echo Attempting to download and extract the latest Vita3K version in progress...
    if ! curl -fL "https://github.com/Vita3K/Vita3K/releases/download/continuous/$zip_name" -o "./vita3k-latest.zip.part" \
        || ! mv -f "./vita3k-latest.zip.part" "./vita3k-latest.zip"; then
        echo "Download or archive finalization failed." >&2
        if ! rm -f "./vita3k-latest.zip.part"; then
            echo "Failed to remove the incomplete download." >&2
        fi
        exit 1
    fi
else
    boot=1
fi

echo Installing update ...
if ! unzip -q -o "./vita3k-latest.zip"; then
    echo "Extraction failed." >&2
    exit 1
fi

# Restore execution permissions
if ! chmod +x ./Vita3K ./update-vita3k.sh; then
    echo "Failed to restore execution permissions." >&2
    exit 1
fi

# Remove the zip after extraction
if ! rm -f "./vita3k-latest.zip"; then
    echo "Failed to remove the update archive." >&2
    exit 1
fi

echo Vita3K updated successfully!

# Start or prompt
if [ $boot -eq 1 ]; then
    echo Starting Vita3K...
    ./Vita3K
else
    read -p "Press [Enter] key to continue..."
fi
