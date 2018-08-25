
IF "%CI%"=="" (
	mkdir src/external/boost-build
	cd src/external/boost
	b2 -j5 --stagedir=../boost-build stage
	cd ../../..
)

mkdir build-windows
pushd build-windows
cmake -G "Visual Studio 15 2017 Win64" ..
popd
