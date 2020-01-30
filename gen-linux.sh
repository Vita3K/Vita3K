#!/usr/bin/env bash
set -ex

# CI uses pre-built Boost
if [[ -z "${CI}" ]]; then
	# Create build dir
	mkdir -p external/boost-build
	cd external/boost

	# Non-Windows needs to build Boost.Build first
	chmod +x tools/build/src/engine/build.sh
	sh bootstrap.sh

	# Build our Boost subset
	./b2 --ignore-site-config -j5 --build-dir=../boost-build --stagedir=../boost-build stage
	cd ../..
fi

# Generate project files
mkdir -p build-linux
cd build-linux
cmake ..

