#!/usr/bin/env bash
set -euo pipefail

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
    libgstreamer-plugins-extra1.0-0 \
    patchelf \
    xvfb
