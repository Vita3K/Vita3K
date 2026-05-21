#!/usr/bin/env bash
set -euo pipefail

android_api_level="${ANDROID_API_LEVEL:-35}"
android_build_tools="${ANDROID_BUILD_TOOLS:-36.0.0}"
android_ndk_version="${ANDROID_NDK_VERSION:-29.0.14206865}"

sudo apt-get update
sudo apt-get install -y ninja-build

repo_root="$(cd "$(dirname "$0")/.." && pwd)"

if command -v sdkmanager > /dev/null; then
    set +o pipefail
    yes | sdkmanager --licenses > /dev/null
    set -o pipefail
    sdkmanager --install \
        "platforms;android-${android_api_level}" \
        "build-tools;${android_build_tools}" \
        "ndk;${android_ndk_version}"

    sdk_root="${ANDROID_SDK_ROOT:-${ANDROID_HOME:-}}"
    if [[ -n "${sdk_root:-}" ]]; then
        export ANDROID_NDK_HOME="${sdk_root}/ndk/${android_ndk_version}"
        if [[ -n "${GITHUB_ENV:-}" ]]; then
            echo "ANDROID_NDK_HOME=${ANDROID_NDK_HOME}" >> "${GITHUB_ENV}"
        fi
    fi
fi

pushd "$repo_root" > /dev/null
for triplet in arm64-android x64-android; do
    vcpkg install --triplet "$triplet"
done
popd > /dev/null
