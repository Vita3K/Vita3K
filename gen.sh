#! /bin/bash
set -ex

mkdir -p build-macos
cd build-macos
conan install --build missing ../src/external
cmake -G Xcode ..
