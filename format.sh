#!/usr/bin/env bash
set -ex

find vita3k tools/gen-modules tools/native-tool \( -name *.cpp -o -name *.h \) | xargs clang-format -i
