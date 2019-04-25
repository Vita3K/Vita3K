@echo off

echo Please choose your preferred Visual Studio version.
choice /c 79 /n /t 3 /d 7 /m "Press '7' for VS 2017 or '9' for VS 2019: "
if errorlevel 1 set vs_version=Visual Studio 15 2017 Win64
if errorlevel 2 set vs_version=Visual Studio 16 2019

REM CI uses pre-built Boost
IF "%CI%"=="" IF "%vs_version%"=="Visual Studio 15 2017 Win64" (
	REM Create build dir
	mkdir src\external\boost-build
	cd src\external\boost

	REM Build our Boost subset
	b2 -j5 --build-dir=../boost-build --stagedir=../boost-build toolset=msvc stage
	cd ../../..
)

REM Create build folder
mkdir build-windows
pushd build-windows

REM Generate project files
IF "%CI%"=="" (
	cmake -G "%vs_version%" ..
) ELSE (
	cmake -G "%vs_version%" -DCI:BOOL=ON -DCMAKE_CONFIGURATION_TYPES=%CONFIGURATION% ..
)
popd
