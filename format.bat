@echo off
cd src\emulator
for /f %%f in ('dir *.cpp *.h /b/s') do clang-format -i %%f
cd ..\gen-modules
for /f %%f in ('dir *.cpp *.h /b/s') do clang-format -i %%f
cd ..\native-tool
for /f %%f in ('dir *.cpp *.h /b/s') do clang-format -i %%f
cd ..\..
