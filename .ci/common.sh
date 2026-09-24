#!/usr/bin/env bash
set -euo pipefail

require_arg() {
    local value="${1:-}"
    local name="${2:-argument}"
    if [[ -z "$value" ]]; then
        echo "Missing required argument: $name" >&2
        exit 1
    fi
}

git_short_sha() {
    git rev-parse --short HEAD
}

# The interface translations live in their own repository, so CI unpacks the published
# archives into the directories the build already reads.
download_translation_archive() {
    local archive="${1:?missing archive name}"
    local target_dir="${2:?missing target directory}"
    local repo="${VITA3K_TRANSLATIONS_REPO:-nishinji/vita3k_translations}"
    local url="https://github.com/$repo/releases/latest/download/$archive"

    mkdir -p "$target_dir"

    echo "Downloading $archive from $url"
    if ! curl -fsSL --retry 3 --retry-delay 10 -o "$target_dir/$archive" "$url"; then
        echo "Failed to download $archive. Building without it."
        return 0
    fi

    if ! (cd "$target_dir" && cmake -E tar xf "$archive"); then
        echo "Failed to unpack $archive. Building without it."
    fi

    rm -f "$target_dir/$archive"
}

download_qt_translations() {
    download_translation_archive "vita3k-qt-translations.zip" "${1:?missing target directory}"
}

download_android_strings() {
    download_translation_archive "vita3k-android-translations.zip" "${1:?missing target directory}"
}
