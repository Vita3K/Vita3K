#!/usr/bin/env bash
set -ex

# Generate project files for Ninja
cmake -S . -B build-linux -G Ninja -DCMAKE_TOOLCHAIN_FILE=./cmake/toolchain/linux-x64.cmake
