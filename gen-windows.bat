@echo off

echo Please choose your preferred Visual Studio version.
choice /c 97 /n /t 3 /d 7 /m "Press '9' for VS 2019 or '7' for VS 2017: "
if errorlevel 1 set vs_version=Visual Studio 16 2019
if errorlevel 2 set vs_version=Visual Studio 15 2017 Win64

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
	call cmake -G "%vs_version%" ..
) ELSE (
	call cmake -G "%vs_version%" -DCI:BOOL=ON -DCMAKE_CONFIGURATION_TYPES=%CONFIGURATION% ..
)

IF NOT DEFINED CI ( 
	set /p selection="Do you want to build?(Y/n)" 
	IF "%selection%"=="Y" (MSBuild Vita3k.sln /p:Configuration=Release /p:Platform=x64)
	)
popd
