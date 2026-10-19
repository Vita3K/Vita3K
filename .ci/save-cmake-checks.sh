#!/usr/bin/env bash
# Writes the check results of a configured build into <build dir>/cmake-checks.cmake, which
# build-desktop.sh preloads so a later configure with the same CMake files skips those checks.
# They are the cache entries that CMake's check modules describe this way.
# Usage: save-cmake-checks.sh <build dir>
set -euo pipefail

build_dir="${1:?missing build directory}"

awk '
/^\/\/(Have |Test |Result of |CHECK_TYPE_SIZE: )/ { help = substr($0, 3); next }
help != "" && /:INTERNAL=/ {
    split($0, entry, ":INTERNAL=")
    value = entry[2]
    gsub(/[\\"]/, "\\\\&", value)
    gsub(/[\\"]/, "\\\\&", help)
    printf "set(%s \"%s\" CACHE INTERNAL \"%s\")\n", entry[1], value, help
}
{ help = "" }
' "$build_dir/CMakeCache.txt" > "$build_dir/cmake-checks.cmake"
