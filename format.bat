@echo off
if "%CLANG_FORMAT_BIN%"=="" set "CLANG_FORMAT_BIN=clang-format"
cd vita3k
for /f %%f in ('dir *.cpp *.h /b/s') do "%CLANG_FORMAT_BIN%" -i "%%f"
cd ..\tools
for /f %%f in ('dir *.cpp *.h /b/s') do "%CLANG_FORMAT_BIN%" -i "%%f"
cd ..
