@echo off

REM Generate project files for Visual Studio 2019
call cmake -S . -B build-windows -G "Visual Studio 2019 16" -DCMAKE_TOOLCHAIN_FILE=./cmake/toolchain/windows-x64.cmake
