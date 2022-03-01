@echo off

REM Generate project files for your last Visual Studio version you have
call cmake -S . -B build-windows -DCMAKE_TOOLCHAIN_FILE=./cmake/toolchain/windows-x64.cmake
pause
