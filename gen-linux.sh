#!/usr/bin/env bash
set -ex

cmake_args=
CLANG=
command -v clang > /dev/null && CLANG=1

# CI uses pre-built Boost
if [[ -z "${CI}" ]]; then
	# Create build dir
	mkdir -p external/boost-build
	cd external/boost

	chmod +x tools/build/src/engine/build.sh
	sh bootstrap.sh

	# Build our Boost subset
	./b2 --ignore-site-config -j$(nproc) --build-dir=../boost-build --stagedir=../boost-build stage
	cd ../..
fi

if [ "$CLANG" ]; then
	cmake_args="-DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++"
fi

pushd .

# Generate project files
mkdir -p build-linux
cd build-linux
cmake .. -GNinja ${cmake_args}

if [[ -z "${CI}" ]]; then
	read -p "Do you want to build?(Y/n)" selection
	if [ "${selection}"=="Y" ]; then
		ninja
	fi
	popd
fi
