#!/usr/bin/env bash
set -euo pipefail

source "$(dirname "$0")/common.sh"

PRESET=""
CONFIG=""

while [[ $# -gt 0 ]]; do
    case "$1" in
        --preset)
            PRESET="$2"
            shift 2
            ;;
        --config)
            CONFIG="$2"
            shift 2
            ;;
        *)
            echo "Unknown argument: $1" >&2
            exit 1
            ;;
    esac
done

require_arg "$PRESET" "preset"
require_arg "$CONFIG" "config"

export CCACHE_DIR
CCACHE_DIR="$(cygpath -u "C:/msys2-ccache")"

cmake --preset "$PRESET"
cmake --build "build/$PRESET" --config "$CONFIG"

BINDIR="build/${PRESET}/bin/${CONFIG}"
MSYS2_BINDIR="/${MSYSTEM,,}/bin"
for dll in libc++.dll libunwind.dll; do
    if [[ -f "$MSYS2_BINDIR/$dll" ]]; then
        cp "$MSYS2_BINDIR/$dll" "$BINDIR/"
    fi
done
