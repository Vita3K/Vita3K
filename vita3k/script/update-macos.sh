#!/bin/sh
DIR="$(dirname "$0")"
APP_PATH="$(dirname "$0")/../../.."
ARCH=$(uname -m)

if [ "$ARCH" = "arm64" ]; then
  # DMG_NAME=macos-arm64-latest.dmg # unstable
  DMG_NAME=macos-latest.dmg
else
  DMG_NAME=macos-latest.dmg
fi

if [ ! -e "$DIR"/vita3k-latest.dmg ]; then
	if ! curl -fL "https://github.com/Vita3K/Vita3K/releases/download/continuous/$DMG_NAME" -o "$DIR"/vita3k-latest.dmg.part \
		|| ! mv -f "$DIR"/vita3k-latest.dmg.part "$DIR"/vita3k-latest.dmg; then
		echo "Download or image finalization failed." >&2
		if ! rm -f "$DIR"/vita3k-latest.dmg.part; then
			echo "Failed to remove the incomplete download." >&2
		fi
		exit 1
	fi
fi

if ! hdiutil attach "$DIR"/vita3k-latest.dmg; then
	echo "Failed to mount the update image." >&2
	exit 1
fi
if ! cp -Rf -p /Volumes/Vita3K\ Installer/Vita3K.app "$APP_PATH"; then
	echo "Failed to copy the application update." >&2
	if ! diskutil eject 'Vita3K Installer'; then
		echo "Failed to eject the update image." >&2
	fi
	exit 1
fi
if ! diskutil eject 'Vita3K Installer'; then
	echo "Failed to eject the update image." >&2
	exit 1
fi
if ! rm -f "$DIR"/vita3k-latest.dmg; then
	echo "Failed to remove the update image." >&2
	exit 1
fi
xattr -d com.apple.quarantine "$APP_PATH"/Vita3K.app
open "$APP_PATH"/Vita3K.app
