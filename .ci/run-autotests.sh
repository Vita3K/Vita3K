#!/usr/bin/env bash
# Runs the VitaAutotests vpk, built by build-autotests.sh, headless on a Linux build of Vita3K, with
# Vulkan on lavapipe, Mesa's CPU driver.
# Usage: run-autotests.sh <Vita3K binary> <VitaAutotests checkout>
set -euo pipefail

VITA3K="$(realpath "$1")"
AUTOTESTS_DIR="$(realpath "$2")"
TOOLS_DIR="$(realpath "$(dirname "$0")/../tools/autotests")"

# The XDG paths give Vita3K a fresh emulated Vita, with no firmware installed
work_dir="$(mktemp -d)"
export XDG_CONFIG_HOME="$work_dir/config" XDG_DATA_HOME="$work_dir/data" XDG_CACHE_HOME="$work_dir/cache"
export VITA3K
export VITA3K_ARGS="-B Vulkan"
# Only lavapipe, so a runner with a GPU gives the same results
lavapipe_icd="$(ls /usr/share/vulkan/icd.d/lvp_icd*.json | head -n 1)"
export VK_DRIVER_FILES="$lavapipe_icd" VK_ICD_FILENAMES="$lavapipe_icd"

# Four tests at once, one per core of the runner. The system python, where apt put pytest, since the
# setup-python of install-qt-action puts its own first on PATH.
/usr/bin/python3 "$TOOLS_DIR/run.py" --jobs 4 --expectations "$TOOLS_DIR/expectations.txt" "$AUTOTESTS_DIR"
