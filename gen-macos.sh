#!/usr/bin/env bash
set -ex

BOOST_FOUND="$(cmake --find-package -DNAME=Boost -DCOMPILER_ID=GNU -DLANGUAGE=C -DMODE=EXIST)"

# CI uses pre-built Boost
if [[ -z "${CI}" && ${BOOST_FOUND} != "Boost found." ]]; then
	# Create build dir
	mkdir -p external/boost-build
	cd external/boost

	# Non-Windows needs to build Boost.Build first
	chmod +x tools/build/src/engine/build.sh
	sh bootstrap.sh

	# Build our Boost subset
	./b2 -j5 --build-dir=../boost-build --stagedir=../boost-build stage
	cd ../..
fi

# Generate project files
mkdir -p build-macos
cd build-macos
cmake -G Xcode ..
