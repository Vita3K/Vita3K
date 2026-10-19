#!/usr/bin/env bash
set -euo pipefail

HOMEBREW_NO_AUTO_UPDATE=1 HOMEBREW_NO_INSTALL_CLEANUP=1 HOMEBREW_NO_INSTALLED_DEPENDENTS_CHECK=1 \
    brew install ccache create-dmg
echo "$(brew --prefix ccache)/libexec" >> "$GITHUB_PATH"
ccache --set-config=compiler_check=content
