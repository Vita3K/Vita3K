mkdir build-windows
pushd build-windows
conan install --build missing ../src/external
cmake -G "Visual Studio 15 2017 Win64" ..
popd
