@echo off

REM Configure Conan dependencies
call conan install . -b missing -of .deps -pr:h conan/profiles/windows-msvc -pr:b conan/profiles/windows-msvc
call conan install . -b missing -of .deps -pr:h conan/profiles/windows-msvc -pr:b conan/profiles/windows-msvc -s build_type=Debug
call conan install . -b missing -of .deps -pr:h conan/profiles/windows-msvc -pr:b conan/profiles/windows-msvc -s build_type=RelWithDebInfo

REM Generate project files for your last Visual Studio version you have
call cmake -S . -B build
pause
