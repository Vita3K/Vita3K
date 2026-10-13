#!/usr/bin/env bash
set -euo pipefail

source "$(dirname "$0")/common.sh"

PRESET=""
CONFIG=""
IN_ARCH_CONTAINER="false"

while [[ $# -gt 0 ]]; do
    case "$1" in
        --preset)
            PRESET="$2"
            shift 2
            ;;
        --config)
            CONFIG="$2"
            shift 2
            ;;
        --in-arch-container)
            IN_ARCH_CONTAINER="true"
            shift
            ;;
        *)
            echo "Unknown argument: $1" >&2
            exit 1
            ;;
    esac
done

require_arg "$PRESET" "preset"
require_arg "$CONFIG" "config"

QT_ROOT_DIR="${QT_ROOT_DIR:-${Qt6_DIR:-}}"
if [[ "$QT_ROOT_DIR" == */lib/cmake/Qt6 ]]; then
    QT_ROOT_DIR="${QT_ROOT_DIR%/lib/cmake/Qt6}"
fi
if [[ -z "$QT_ROOT_DIR" && -f "build/$PRESET/CMakeCache.txt" ]]; then
    QT6_CMAKE_DIR="$(sed -n 's/^Qt6_DIR:PATH=//p' "build/$PRESET/CMakeCache.txt" | head -n 1)"
    if [[ "$QT6_CMAKE_DIR" == */lib/cmake/Qt6 ]]; then
        QT_ROOT_DIR="${QT6_CMAKE_DIR%/lib/cmake/Qt6}"
    fi
fi

ARCH="$(uname -m)"

# Execute this very script in an arch container, if we are not in a container then set it up
if [[ "$IN_ARCH_CONTAINER" != "true" ]]; then
    REPO_ROOT="$(pwd)"
    SCRIPT_PATH="$REPO_ROOT/.ci/package-appimage.sh"
    DOCKER_ARGS=(
        run --rm
        --volume "$REPO_ROOT:$REPO_ROOT"
        --workdir "$REPO_ROOT"
    )
    if [[ -n "$QT_ROOT_DIR" ]]; then
        DOCKER_ARGS+=(--volume "$QT_ROOT_DIR:$QT_ROOT_DIR" --env "QT_ROOT_DIR=$QT_ROOT_DIR")
    fi

    case $ARCH in
        x86_64)
            DOCKER_ARGS+=(--platform linux/amd64 ghcr.io/pkgforge-dev/archlinux:latest)
            ;;
        aarch64)
            DOCKER_ARGS+=(--platform linux/arm64 ghcr.io/pkgforge-dev/archlinux:latest)
            ;;
        *)
            echo "Unsupported AppImage architecture: $ARCH" >&2
            exit 1
            ;;
    esac

    docker "${DOCKER_ARGS[@]}" bash "$SCRIPT_PATH" \
        --preset "$PRESET" \
        --config "$CONFIG" \
        --in-arch-container
    exit 0
fi
# Everything below this ONLY runs in an arch container
pacman-key --init
pacman-key --populate
pacman -Syu --noconfirm --needed \
    base-devel \
    cmake \
    curl \
    patchelf \
    qt6-base \
    qt6-multimedia \
    qt6-svg \
    git \
    xorg-server-xvfb

git config --global --add safe.directory "$(pwd)"

case $ARCH in
    x86_64|aarch64)
    # do Nothing
        ;;
    *)
        echo "Unsupported AppImage architecture: $ARCH" >&2
        exit 1
        ;;
esac

INSTALL_PREFIX="/usr" # This comes from the cmake presets, which default to /usr
cmake --install "build/$PRESET" --config "$CONFIG" --prefix "$INSTALL_PREFIX"

TEMP_DIR="$(mktemp -d)"
APPDIR="$TEMP_DIR/AppDir"
OUTPUT_DIR="build/$PRESET/bin/$CONFIG"

export APPDIR
export ARCH
export OUTPATH="$OUTPUT_DIR"
export ADD_HOOKS="self-updater.hook"
export UPINFO="gh-releases-zsync|Vita3K|Vita3K|continuous|Vita3K-$ARCH.AppImage.zsync"
export ICON="$INSTALL_PREFIX/share/icons/hicolor/scalable/apps/org.vita3k.vita3k.svg"
export DESKTOP="$INSTALL_PREFIX/share/applications/org.vita3k.vita3k.desktop"
export DEPLOY_OPENGL=1
export DEPLOY_VULKAN=1
export DEPLOY_PULSE=1
export OUTNAME="Vita3K-$ARCH.AppImage"


DEBLOATED_PACKAGES_URL="https://raw.githubusercontent.com/pkgforge-dev/Anylinux-AppImages/refs/heads/main/useful-tools/get-debloated-pkgs.sh"
DEBLOATED_PACKAGES="$(mktemp)"
trap 'rm -f "$DEBLOATED_PACKAGES"' EXIT
curl -fsSL --retry 3 --retry-delay 10 "$DEBLOATED_PACKAGES_URL" -o "$DEBLOATED_PACKAGES"
chmod +x "$DEBLOATED_PACKAGES"
"$DEBLOATED_PACKAGES" --add-mesa --prefer-nano ffmpeg-mini

printf "==== Done installing debloated packages ====\n"

QUICK_SHARUN_URL="https://raw.githubusercontent.com/pkgforge-dev/Anylinux-AppImages/refs/heads/main/useful-tools/quick-sharun.sh"
trap 'rm -rf "$TEMP_DIR"' EXIT
QUICK_SHARUN="$TEMP_DIR/quick-sharun.sh"

curl -fsSL --retry 3 --retry-delay 10 "$QUICK_SHARUN_URL" -o "$QUICK_SHARUN"
chmod +x "$QUICK_SHARUN"

mkdir --parents $APPDIR
echo "r$(git rev-list --count HEAD).$(git rev-parse --short HEAD)" > $APPDIR/version
"$QUICK_SHARUN" "$INSTALL_PREFIX/bin/Vita3K" "$INSTALL_PREFIX/share/Vita3K"
test -d "$APPDIR/share/Vita3K"
printf "==== Making AppImage... ====\n"
"$QUICK_SHARUN" --make-appimage
