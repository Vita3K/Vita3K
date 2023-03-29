#!/bin/sh
if [ ! -e vita3k-latest.dmg ]; then
    curl -L https://github.com/Vita3K/Vita3K/releases/download/continuous/macos-latest.dmg -o vita3k-latest.dmg
fi

APP_PATH='./../../..'

hdiutil attach vita3k-latest.dmg
cp -Rf -p /Volumes/Vita3K\ Installer/Vita3K.app $APP_PATH
diskutil eject Vita3K\ Installer
rm -f vita3k-latest.dmg
xattr -d com.apple.quarantine $APP_PATH/Vita3K.app
open $APP_PATH/Vita3K.app
