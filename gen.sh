#! /bin/bash
set -ex

if [[ -z "${CI}" ]]; then
	mkdir src/external/boost-build
	cd src/external/boost
	bootstrap.sh   # Non-windows needs to build Boost.Build first (but b2.exe is already in ext-boost)
	b2 -j5 --build-dir=../boost-build
	cd ../../..
fi


mkdir -p build-macos
cd build-macos
cmake -G Xcode ..
