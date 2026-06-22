#!/usr/bin/env bash
set -euo pipefail

sudo xcode-select -s /Applications/Xcode_16.2.app/Contents/Developer
brew install ccache create-dmg
echo "$(brew --prefix ccache)/libexec" >> "$GITHUB_PATH"
ccache --set-config=compiler_check=content
