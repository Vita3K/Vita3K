#!/bin/sh
[ ! -e ~/vita3k-latest.dmg ] && curl -L https://github.com/Vita3K/Vita3K/releases/download/continuous/macos-latest.dmg -o ~/vita3k-latest.dmg

hdiutil attach ~/vita3k-latest.dmg
cp -Rf -p /Volumes/Vita3K\ Installer/Vita3K.app $1
diskutil eject Vita3K\ Installer
rm -f ~/vita3k-latest.dmg
xattr -d com.apple.quarantine $1/Vita3K.app
open $1/Vita3K.app
