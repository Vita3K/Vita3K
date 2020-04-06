#!/usr/bin/env bash
set -ex

# Generate project files
mkdir -p build-linux
cd build-linux
cmake ..

