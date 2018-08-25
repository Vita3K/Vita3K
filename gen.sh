#! /bin/bash
set -ex

if [[ -z "${CI}" ]]; then
	mkdir src/external/boost-build
	cd src/external/boost
	b2 -j5 --build-dir=../boost-build
	cd ../../..
fi


mkdir -p build-macos
cd build-macos
cmake -G Xcode ..
