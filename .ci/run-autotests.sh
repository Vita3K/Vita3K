#!/usr/bin/env bash
# Builds the VitaAutotests vpk and runs it headless on a Linux build of Vita3K, with Vulkan on lavapipe,
# Mesa's CPU driver.
# Usage: run-autotests.sh <Vita3K binary> <VitaAutotests checkout>
set -euo pipefail

VITA3K="$(realpath "$1")"
AUTOTESTS_DIR="$(realpath "$2")"
TOOLS_DIR="$(realpath "$(dirname "$0")/../tools/autotests")"

# CI restores the vpk built from the same autotests commit
if [[ "${AUTOTESTS_VPK_CACHED:-}" != true ]]; then
    docker run --rm --user "$(id -u):$(id -g)" -v "$AUTOTESTS_DIR:/src" -w /src vitasdk/vitasdk:latest \
        sh -c 'cmake -B build && cmake --build build -j"$(nproc)"'
fi

sudo apt-get -y install mesa-vulkan-drivers

export VITA3K
export VITA3K_ARGS="-B Vulkan"
# Only lavapipe, so a runner with a GPU gives the same results
lavapipe_icd="$(ls /usr/share/vulkan/icd.d/lvp_icd*.json | head -n 1)"
export VK_DRIVER_FILES="$lavapipe_icd" VK_ICD_FILENAMES="$lavapipe_icd"

work_dir="$(mktemp -d)"

# Runs the named tests one by one in a fresh emulated Vita in <dir>, with no firmware installed
run_tests() {
    local dir="$1"
    shift
    XDG_CONFIG_HOME="$dir/config" XDG_DATA_HOME="$dir/data" XDG_CACHE_HOME="$dir/cache" \
        python3 "$TOOLS_DIR/run.py" --expectations "$TOOLS_DIR/expectations.txt" "$AUTOTESTS_DIR" "$@"
}

# The tests are split into shards that run at once. More shards than half the cores make timing
# tests like lw_mutex_semantics miss their expectation more often.
shards="${AUTOTESTS_SHARDS:-$(( $(nproc) / 2 > 1 ? $(nproc) / 2 : 1 ))}"
mapfile -t names < <(find "$AUTOTESTS_DIR/tests" -name '*.expected' -printf '%f\n' | sed 's/\.expected$//' | sort)
if [[ ${#names[@]} -eq 0 ]]; then
    echo "No tests in $AUTOTESTS_DIR" >&2
    exit 1
fi
# A shard without names would run every test
if (( shards > ${#names[@]} )); then
    shards=${#names[@]}
fi
pids=()
for ((shard = 0; shard < shards; shard++)); do
    shard_names=()
    for ((i = shard; i < ${#names[@]}; i += shards)); do
        shard_names+=("${names[i]}")
    done
    run_tests "$work_dir/$shard" "${shard_names[@]}" > "$work_dir/$shard.log" 2>&1 &
    pids+=($!)
done

status=0
retry=()
for ((shard = 0; shard < shards; shard++)); do
    if ! wait "${pids[shard]}"; then
        mapfile -t unexpected < <(sed -n 's/^UNEXPECTED \([^:]*\):.*/\1/p' "$work_dir/$shard.log")
        # Only a shard that ran all its tests, up to its summary, can be settled by retries
        if [[ ${#unexpected[@]} -gt 0 ]] && tail -n 1 "$work_dir/$shard.log" | grep -Eq '^[0-9]+ passed, .* unexpected$'; then
            retry+=("${unexpected[@]}")
        else
            status=1
        fi
    fi
    echo "=== Shard $((shard + 1)) of $shards"
    cat "$work_dir/$shard.log"
done

# The shards load the host, so a test that missed its expectation counts by how it does on its own
if [[ ${#retry[@]} -gt 0 ]]; then
    echo "=== Again on their own: ${retry[*]}"
    run_tests "$work_dir/retry" "${retry[@]}" || status=1
fi
exit "$status"
