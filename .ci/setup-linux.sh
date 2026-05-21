#!/usr/bin/env bash
set -euo pipefail

ARCH="${1:-x86_64}"
APPIMAGE_MODE="${2:-with-appimage}"

sudo add-apt-repository -y ppa:mhier/libboost-latest
sudo add-apt-repository universe
sudo apt update
sudo apt -y install \
    libboost-filesystem1.83-dev \
    libboost-program-options1.83-dev \
    libboost-system1.83-dev \
    libgtk-3-dev \
    ninja-build \
    libfuse2 \
    gnome-desktop-testing \
    libasound2-dev \
    libpulse-dev \
    libaudio-dev \
    libfribidi-dev \
    libjack-dev \
    libsndio-dev \
    libx11-dev \
    libxext-dev \
    libxrandr-dev \
    libxcursor-dev \
    libxfixes-dev \
    libxi-dev \
    libxss-dev \
    libxtst-dev \
    libxkbcommon-dev \
    libdrm-dev \
    libgbm-dev \
    libgl1-mesa-dev \
    libgles2-mesa-dev \
    libegl1-mesa-dev \
    libdbus-1-dev \
    libibus-1.0-dev \
    libudev-dev \
    libthai-dev \
    libpipewire-0.3-dev \
    libwayland-dev \
    libdecor-0-dev \
    liburing-dev \
    libgstreamer-plugins-bad1.0-0 \
    libgstreamer-plugins-good1.0-0

if [[ "$APPIMAGE_MODE" == "with-appimage" ]]; then
    curl -sLO "https://github.com/linuxdeploy/linuxdeploy/releases/latest/download/linuxdeploy-${ARCH}.AppImage"
    sudo cp -f "linuxdeploy-${ARCH}.AppImage" "/usr/local/bin/"
    sudo chmod +x "/usr/local/bin/linuxdeploy-${ARCH}.AppImage"

    curl -sLO "https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/latest/download/linuxdeploy-plugin-qt-${ARCH}.AppImage"
    sudo cp -f "linuxdeploy-plugin-qt-${ARCH}.AppImage" "/usr/local/bin/"
    sudo chmod +x "/usr/local/bin/linuxdeploy-plugin-qt-${ARCH}.AppImage"
fi
