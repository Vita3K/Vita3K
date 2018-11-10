#!/usr/bin/env bash

apt-get update
apt-get install -y curl gcc-8 g++-8 libboost-all-dev libsdl2-dev ccache

mkdir tmp && cd tmp
curl -Ls https://www.archlinux.org/packages/extra/x86_64/sdl2/download | tar xJ
cd .. && rm -rf tmp

update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-8 90
update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-8 90
curl -Ls https://cmake.org/files/v3.10/cmake-3.10.3-Linux-x86_64.sh -o cmake-3.10.3-Linux-x86_64.sh
bash cmake-3.10.3-Linux-x86_64.sh --skip-license

export CMAKE=$(pwd)bin/cmake

cd /Vita3K

mkdir build
cd build/
${CMAKE} -DCMAKE_BUILD_TYPE=Release -DCI:BOOL=ON -DCMAKE_C_COMPILER=/usr/lib/ccache/gcc -DCMAKE_CXX_COMPILER=/usr/lib/ccache/g++ ..
make -j4
make CTEST_OUTPUT_ON_FAILURE=1 test