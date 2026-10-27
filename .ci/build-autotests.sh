#!/usr/bin/env bash
# Builds the VitaAutotests vpk in the vitasdk container.
# Usage: build-autotests.sh <VitaAutotests checkout>
set -euo pipefail

AUTOTESTS_DIR="$(realpath "$1")"

docker run --rm --user "$(id -u):$(id -g)" -v "$AUTOTESTS_DIR:/src" -w /src vitasdk/vitasdk:latest \
    sh -c 'cmake -B build && cmake --build build -j"$(nproc)"'
