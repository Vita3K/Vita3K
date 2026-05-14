#!/bin/sh
DIR="$(dirname "$0")"
APP_PATH="$(dirname "$0")/../../.."

if [ ! -e "$DIR"/RPCSV-latest.dmg ]; then
	curl -L https://github.com/RPCSV/RPCSV/releases/download/continuous/macos-latest.dmg -o "$DIR"/RPCSV-latest.dmg
fi

hdiutil attach "$DIR"/RPCSV-latest.dmg
cp -Rf -p /Volumes/RPCSV\ Installer/RPCSV.app "$APP_PATH"
diskutil eject 'RPCSV Installer'
rm -f "$DIR"/RPCSV-latest.dmg
xattr -d com.apple.quarantine "$APP_PATH"/RPCSV.app
open "$APP_PATH"/RPCSV.app
