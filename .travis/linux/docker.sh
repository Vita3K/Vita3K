#!/usr/bin/env bash

apt-get update
apt-get install -y ccache libboost-filesystem1.67-dev libboost-program-options1.67-dev libboost-system1.67-dev libsdl2-dev

curl -Ls https://cmake.org/files/v3.10/cmake-3.10.3-Linux-x86_64.sh -o cmake-3.10.3-Linux-x86_64.sh
bash cmake-3.10.3-Linux-x86_64.sh --skip-license

export CMAKE=$(pwd)bin/cmake

cd /Vita3K

mkdir build
cd build/
${CMAKE} -DCMAKE_BUILD_TYPE=Release -DCI:BOOL=ON ..
make -j4
make CTEST_OUTPUT_ON_FAILURE=1 test
