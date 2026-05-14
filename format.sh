#!/usr/bin/env bash
set -ex

find RPCSV tools/gen-modules tools/native-tool \( -name *.cpp -o -name *.h \) | xargs clang-format -i
