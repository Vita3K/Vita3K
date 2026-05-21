#!/usr/bin/env bash
set -euo pipefail

sudo apt-get update
sudo apt-get install -y ninja-build

repo_root="$(cd "$(dirname "$0")/.." && pwd)"

pushd "$repo_root" > /dev/null
for triplet in arm64-android x64-android; do
    vcpkg install --triplet "$triplet"
done
popd > /dev/null
