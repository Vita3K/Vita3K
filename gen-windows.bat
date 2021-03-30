@echo off

REM CI uses pre-built Boost
IF "%CI%"=="" IF NOT EXIST external\boost-build (
	REM Create build dir
	mkdir external\boost-build
	cd external\boost

	call bootstrap.bat

	REM Build our Boost subset
	call b2 -j%NUMBER_OF_PROCESSORS% --build-dir=../boost-build --stagedir=../boost-build toolset=msvc stage
	cd ../..
)

REM Create build folder
mkdir build-windows
pushd build-windows

REM Generate project files
IF "%CI%"=="" (
	call cmake -G "Visual Studio 16 2019" ..
) ELSE (
	call cmake -G "Visual Studio 16 2019" -DCI:BOOL=ON -DCMAKE_CONFIGURATION_TYPES=%CONFIGURATION% ..
)
popd
