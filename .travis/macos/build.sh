#!/usr/bin/env bash

mkdir build
cd build/
${CMAKE} -DCMAKE_BUILD_TYPE=Release -DCI:BOOL=ON ..
make -j4
make CTEST_OUTPUT_ON_FAILURE=1 test
