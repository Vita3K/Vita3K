#!/usr/bin/env bash
set -euo pipefail

CLANG_FORMAT_BIN="${CLANG_FORMAT_BIN:-clang-format}"

find vita3k tools/gen-modules tools/native-tool \( -name '*.cpp' -o -name '*.h' \) -print0 | xargs -0 "$CLANG_FORMAT_BIN" -i
