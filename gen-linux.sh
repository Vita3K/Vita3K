#!/usr/bin/env bash
set -ex

# Generate project files for Ninja
cmake --preset linux-ninja-clang
