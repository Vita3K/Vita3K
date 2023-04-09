#!/usr/bin/env bash
set -ex

# Configure Conan dependencies
conan install . -b missing -of .deps -pr:h conan/profiles/linux-clang_libstdc++ -pr:b conan/profiles/linux-clang_libstdc++
conan install . -b missing -of .deps -pr:h conan/profiles/linux-clang_libstdc++ -pr:b conan/profiles/linux-clang_libstdc++ -s build_type=Debug
conan install . -b missing -of .deps -pr:h conan/profiles/linux-clang_libstdc++ -pr:b conan/profiles/linux-clang_libstdc++ -s build_type=RelWithDebInfo

# Generate project files for Ninja
cmake --preset linux-ninja-clang
