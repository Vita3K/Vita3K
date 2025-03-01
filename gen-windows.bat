@echo off
title Generate Vita3K project files

REM Generate project files for your last Visual Studio version you have
call cmake -S . -B build
pause
