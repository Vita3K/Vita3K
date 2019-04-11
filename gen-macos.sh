#! /bin/bash
set -ex

# CI uses pre-built Boost
if [[ -z "${CI}" && "$1" -ne "brew" ]]; then
	# Create build dir
	mkdir -p src/external/boost-build
	cd src/external/boost

	# Non-Windows needs to build Boost.Build first
	chmod +x tools/build/src/engine/build.sh
	sh bootstrap.sh

	# Build our Boost subset
	./b2 -j5 --build-dir=../boost-build --stagedir=../boost-build stage
	cd ../../..
fi

# Generate project files
mkdir -p build-macos
cd build-macos
cmake -G Xcode ..
