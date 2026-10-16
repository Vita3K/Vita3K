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
download_qt_translations "build/$PRESET/qt_translations"
cmake --build "build/$PRESET" --config "$CONFIG"

BINDIR="build/${PRESET}/bin/${CONFIG}"
MSYS2_BINDIR="/${MSYSTEM,,}/bin"
mapfile -t dlls < <(ldd "$BINDIR/Vita3K.exe" | grep -oiE "$MSYS2_BINDIR/\S+\.dll" | sort -u)
echo "Bundling MSYS2 DLLs:"
printf '%s\n' "${dlls[@]}"
for dll in "${dlls[@]}"; do
    cp "$dll" "$BINDIR/"
done
