#! /bin/bash
set -ex

find src/vita3k src/gen-modules src/native-tool \( -name *.cpp -o -name *.h \) | xargs clang-format -i
