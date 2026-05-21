#!/usr/bin/env bash
set -euo pipefail

source "$(dirname "$0")/common.sh"

PLATFORM=""
PRESET=""
CONFIG=""
OUTPUT_DIR=""

while [[ $# -gt 0 ]]; do
    case "$1" in
        --platform)
            PLATFORM="$2"
            shift 2
            ;;
        --preset)
            PRESET="$2"
            shift 2
            ;;
        --config)
            CONFIG="$2"
            shift 2
            ;;
        --output-dir)
            OUTPUT_DIR="$2"
            shift 2
            ;;
        *)
            echo "Unknown argument: $1" >&2
            exit 1
            ;;
    esac
done

require_arg "$PLATFORM" "platform"
require_arg "$PRESET" "preset"
require_arg "$CONFIG" "config"
require_arg "$OUTPUT_DIR" "output-dir"

build_dir="build/$PRESET/bin/$CONFIG"
repo_root="$(pwd)"
mkdir -p "$OUTPUT_DIR"

case "$PLATFORM" in
    linux-*)
        mkdir -p "$OUTPUT_DIR/bin"
        shopt -s dotglob nullglob
        for path in "$build_dir"/*; do
            base_name="$(basename "$path")"
            if [[ "$base_name" == *AppImage* ]]; then
                cp -R "$path" "$OUTPUT_DIR/"
            else
                cp -R "$path" "$OUTPUT_DIR/bin/"
            fi
        done
        shopt -u dotglob nullglob
        ;;
    macos-*)
        dmg_name="vita3k-${GIT_SHORT_SHA:-$(git_short_sha)}-${PLATFORM}.dmg"
        pushd "$build_dir" > /dev/null
        create-dmg \
            --volname "Vita3K Installer" \
            --volicon Vita3K.app/Contents/Resources/Vita3K.icns \
            --window-size 500 300 \
            --icon-size 100 \
            --icon Vita3K.app 120 115 \
            --app-drop-link 360 115 \
            "$dmg_name" \
            Vita3K.app
        mv -f "$dmg_name" "$repo_root/$OUTPUT_DIR/"
        rm -rf Vita3K.app
        popd > /dev/null
        ;;
    windows-*)
        mkdir -p "$OUTPUT_DIR/bin"
        cp -R "$build_dir/." "$OUTPUT_DIR/bin/"
        ;;
    *)
        echo "Unsupported desktop platform: $PLATFORM" >&2
        exit 1
        ;;
esac
