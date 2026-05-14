@echo off
cd RPCSV
for /f %%f in ('dir *.cpp *.h /b/s') do clang-format -i %%f
cd ..\tools
for /f %%f in ('dir *.cpp *.h /b/s') do clang-format -i %%f
cd ..
