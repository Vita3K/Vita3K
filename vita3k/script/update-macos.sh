#!/bin/sh
DIR="$(dirname "$0")"
APP_PATH="$(dirname "$0")/../../.."

if [ ! -e "$DIR"/vita3k-latest.dmg ]; then
	curl -L https://github.com/Vita3K/Vita3K/releases/download/continuous/macos-latest.dmg -o "$DIR"/vita3k-latest.dmg
fi

hdiutil attach "$DIR"/vita3k-latest.dmg
cp -Rf -p /Volumes/Vita3K\ Installer/Vita3K.app "$APP_PATH"
diskutil eject 'Vita3K Installer'
rm -f "$DIR"/vita3k-latest.dmg
xattr -d com.apple.quarantine "$APP_PATH"/Vita3K.app
open "$APP_PATH"/Vita3K.app
