#!/usr/bin/env bash
# Builds the VitaAutotests vpk and runs it headless on a Linux build of Vita3K, with Vulkan on lavapipe,
# Mesa's CPU driver.
# Usage: run-autotests.sh <Vita3K binary> <VitaAutotests checkout>
set -euo pipefail

VITA3K="$(realpath "$1")"
AUTOTESTS_DIR="$(realpath "$2")"
TOOLS_DIR="$(realpath "$(dirname "$0")/../tools/autotests")"

docker run --rm --user "$(id -u):$(id -g)" -v "$AUTOTESTS_DIR:/src" -w /src vitasdk/vitasdk:latest \
    sh -c 'cmake -B build && cmake --build build -j"$(nproc)"'

sudo apt-get -y install mesa-vulkan-drivers

# The XDG paths give Vita3K a fresh emulated Vita, with no firmware installed
work_dir="$(mktemp -d)"
export XDG_CONFIG_HOME="$work_dir/config" XDG_DATA_HOME="$work_dir/data" XDG_CACHE_HOME="$work_dir/cache"
export VITA3K
export VITA3K_ARGS="-B Vulkan"
# Only lavapipe, so a runner with a GPU gives the same results
lavapipe_icd="$(ls /usr/share/vulkan/icd.d/lvp_icd*.json | head -n 1)"
export VK_DRIVER_FILES="$lavapipe_icd" VK_ICD_FILENAMES="$lavapipe_icd"

python3 "$TOOLS_DIR/run.py" --expectations "$TOOLS_DIR/expectations.txt" "$AUTOTESTS_DIR"
