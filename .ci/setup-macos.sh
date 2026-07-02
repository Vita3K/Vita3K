#!/usr/bin/env bash
set -euo pipefail

brew install ccache create-dmg
echo "$(brew --prefix ccache)/libexec" >> "$GITHUB_PATH"
ccache --set-config=compiler_check=content
