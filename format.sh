#! /bin/bash
set -ex

find src/emulator src/gen-modules src/native-tool \( -name *.cpp -o -name *.h \) | xargs clang-format -i
