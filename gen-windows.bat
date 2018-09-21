@echo off

REM CI uses pre-built Boost
IF "%CI%"=="" (
	REM Create build dir
	mkdir src\external\boost-build
	cd src\external\boost

	REM Build our Boost subset
	b2 -j5 --build-dir=../boost-build --stagedir=../boost-build stage
	cd ../../..
)

REM Generate project files
mkdir build-windows
pushd build-windows

IF "%CI%"=="" (
	cmake -G "Visual Studio 15 2017 Win64" ..
) ELSE (
	cmake -G "Visual Studio 15 2017 Win64" -DCI:BOOL=ON ..
)
popd
