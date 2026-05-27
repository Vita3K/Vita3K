#!/usr/bin/env bash
set -euo pipefail

mkdir -p artifacts-master artifacts-store

repo_root="$(pwd)"
artifacts_master_dir="$repo_root/artifacts-master"
artifacts_store_dir="$repo_root/artifacts-store"

mapfile -t artifact_dirs < <(find . -mindepth 1 -maxdepth 1 -type d -name 'vita3k-*' -print | sort)

for dir in "${artifact_dirs[@]}"; do
    abs_dir="$(cd "$dir" && pwd)"
    bin_dir="$abs_dir/bin"
    artifact_name="$(basename "$abs_dir")"

    case "$artifact_name" in
        vita3k-*-android)
            cp "$abs_dir/app.apk" "$artifacts_master_dir/android-latest.apk"
            cp "$abs_dir/app.apk" "$artifacts_store_dir/vita3k-${BUILD_VARIABLE}-${GIT_SHORT_SHA}-android.apk"
            ;;
        vita3k-*-macos-x64)
            cp "$abs_dir"/*.dmg "$artifacts_master_dir/macos-latest.dmg"
            cp "$abs_dir"/*.dmg "$artifacts_store_dir/vita3k-${BUILD_VARIABLE}-${GIT_SHORT_SHA}-macos-intel.dmg"
            ;;
        vita3k-*-macos-arm64)
            cp "$abs_dir"/*.dmg "$artifacts_master_dir/macos-arm64-latest.dmg"
            cp "$abs_dir"/*.dmg "$artifacts_store_dir/vita3k-${BUILD_VARIABLE}-${GIT_SHORT_SHA}-macos-arm64.dmg"
            ;;
        vita3k-*-linux-x64)
            cp "$abs_dir"/*.AppImage* "$artifacts_master_dir/"
            cp "$abs_dir"/*.AppImage* "$artifacts_store_dir/"
            (cd "$bin_dir" && zip -r "$artifacts_master_dir/ubuntu-latest.zip" .)
            (cd "$bin_dir" && 7z a -mx=9 "$artifacts_store_dir/vita3k-${BUILD_VARIABLE}-${GIT_SHORT_SHA}-ubuntu-x86_64.7z" .)
            ;;
        vita3k-*-linux-arm64)
            cp "$abs_dir"/*.AppImage* "$artifacts_master_dir/"
            cp "$abs_dir"/*.AppImage* "$artifacts_store_dir/"
            (cd "$bin_dir" && zip -r "$artifacts_master_dir/ubuntu-aarch64-latest.zip" .)
            (cd "$bin_dir" && 7z a -mx=9 "$artifacts_store_dir/vita3k-${BUILD_VARIABLE}-${GIT_SHORT_SHA}-ubuntu-aarch64.7z" .)
            ;;
        vita3k-*-windows-x64)
            (cd "$bin_dir" && zip -r "$artifacts_master_dir/windows-latest.zip" .)
            (cd "$bin_dir" && 7z a -mx=9 "$artifacts_store_dir/vita3k-${BUILD_VARIABLE}-${GIT_SHORT_SHA}-windows-x86_64.7z" .)
            ;;
        vita3k-*-windows-arm64)
            (cd "$bin_dir" && zip -r "$artifacts_master_dir/windows-arm64-latest.zip" .)
            (cd "$bin_dir" && 7z a -mx=9 "$artifacts_store_dir/vita3k-${BUILD_VARIABLE}-${GIT_SHORT_SHA}-windows-arm64.7z" .)
            ;;
        *)
            echo "Unknown artifact directory: $artifact_name" >&2
            exit 1
            ;;
    esac
done

echo "=== artifacts-master ==="
ls -al artifacts-master/
echo "=== artifacts-store ==="
ls -al artifacts-store/
