#!/usr/bin/env bash
set -euo pipefail

source "$(dirname "$0")/common.sh"

PRESET=""
CONFIG=""
RUN_TESTS="true"

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
        --skip-tests)
            RUN_TESTS="false"
            shift
            ;;
        *)
            echo "Unknown argument: $1" >&2
            exit 1
            ;;
    esac
done

require_arg "$PRESET" "preset"
require_arg "$CONFIG" "config"

cmake --preset "$PRESET"
cmake --build "build/$PRESET" --config "$CONFIG"

if [[ "$RUN_TESTS" == "true" ]]; then
    ctest --test-dir "build/$PRESET" --build-config "$CONFIG" --output-on-failure
fi
