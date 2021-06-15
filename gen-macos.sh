#!/usr/bin/env bash
set -ex

# Generate project files for Xcode
cmake -S . -B build-macos -G Xcode -DCMAKE_TOOLCHAIN_FILE=./cmake/toolchain/macos-x64.cmake
