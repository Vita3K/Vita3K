#! /bin/bash
set -ex

mkdir -p build-macos
cd build-macos
cmake -G Xcode ../src
