@echo off
cd vita3k
for /f %%f in ('dir *.cpp *.h /b/s') do clang-format -i %%f
cd ..\tools
for /f %%f in ('dir *.cpp *.h /b/s') do clang-format -i %%f
cd ..
