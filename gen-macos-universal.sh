#! /usr/bin/env zsh
setopt sh_word_split

# Select cmake generator
if [ "$1" = "xcode" ] ; then
    generator="macos-xcode"
elif [ "$1" = "ninja" ] ; then
    generator="macos-ninja"
else
    echo "Fallbacking to ninja"
    generator="macos-ninja"
fi

# Detect system architecture
arch=$(arch)
echo "System arch is $arch"
if [ "$arch" = "arm64" ] ; then
    diff_arch="x86_64"
else
    arch="x86_64"
    diff_arch="arm64"
fi

# Prepare homebrew for other arch
echo "Preparing $diff_arch Homebrew"
if [ ! -f "$(pwd)/build/$diff_arch-homebrew/bin/brew" ] ; then
    mkdir -p ./build/$diff_arch-homebrew && curl -L https://github.com/Homebrew/brew/tarball/master | tar xz --strip 1 -C ./build/$diff_arch-homebrew
fi
alias diff-brew='$(pwd)/build/$diff_arch-homebrew/bin/brew'

function lipo_brew_lib() {
    echo "Making $1's fat binaries"
	original_brew=$(brew --prefix $1)/lib
	diff_arch_brew=$(diff-brew --prefix $1)/lib
	cp -R $original_brew/.. build/fat-homebrew-lib/$1
    
    # *.a
    for f in $original_brew/*.a
	do
		filename=$(basename $f)
		lipo -create -output build/fat-homebrew-lib/$1/lib/$filename $original_brew/$filename $diff_arch_brew/$filename
	done
 
    # *.dylib
    for f in $original_brew/*.dylib
    do
        filename=$(basename $f)
        lipo -create -output build/fat-homebrew-lib/$1/lib/$filename $original_brew/$filename $diff_arch_brew/$filename
    done
}

function install_brew_lib() {
    if [ "$3" = "reinstall" ] ; then
        install_type="reinstall"
    else
        install_type="install"
    fi
    echo "Installing $2 for $1"
    if [ "$1" = "$diff_arch" ] ; then
	    response=$(diff-brew fetch --force --bottle-tag=${1}_monterey $2 | grep "Downloaded to")
        parsed=($response)  
        diff-brew $install_type $parsed[3]
    else
	    response=$(brew fetch --force --bottle-tag=${1}_monterey $2 | grep "Downloaded to")
        parsed=($response)  
        brew $install_type $parsed[3]
    fi
}

# Install diff arch libraries
echo "Installing $arch libraries"
install_brew_lib $arch boost # Install boost
install_brew_lib $arch openssl@3 reinstall # Install openssl@3

echo "Installing $diff_arch libraries"
install_brew_lib $diff_arch boost # Install boost
install_brew_lib $diff_arch openssl@3 # Install openssl@3

# Create fat binaries for libraries that installed by Homebrew
echo "Making fat binaries"
mkdir -p ./build/fat-homebrew-lib
lipo_brew_lib boost
lipo_brew_lib openssl@3

# Build universal binary for libtomcrypt
echo "Compile fat binaries for libtomcrypt"
cd external/psvpfstools/libtomcrypt
make clean
flags="-mmacosx-version-min=11.0 -arch arm64 -arch x86_64"
CC=$(brew --prefix ccache)/libexec/cc make CFLAGS="$flags" LDFLAGS="$flags"
\cp -f libtomcrypt.a build/lib/tomcrypt.a
cd ../../..

# Build universal binary for Unicorn
echo "Compile universal binaries for Unicorn"
cd external/unicorn
git apply ./../../unicorn.diff
cmake . -DTARGET_ARCH=aarch64 -DBUILD_SHARED_LIBS=OFF -DUNICORN_BUILD_TESTS=OFF -DUNICORN_INSTALL=OFF -DUNICORN_ARCH=arm -DCMAKE_OSX_ARCHITECTURES="arm64" -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0 -B unicorn-arm64
cmake --build unicorn-arm64
cmake . -DTARGET_ARCH=x86_64 -DBUILD_SHARED_LIBS=OFF -DUNICORN_BUILD_TESTS=OFF -DUNICORN_INSTALL=OFF -DUNICORN_ARCH=arm -DCMAKE_OSX_ARCHITECTURES="x86_64" -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0 -B unicorn-x86_64
cmake --build unicorn-x86_64
for f in unicorn-arm64/*.a
do
    filename=$(basename $f)
    lipo -create -output $filename unicorn-arm64/$filename unicorn-x86_64/$filename
done
cd ../..

# Build universal binary for xxHash
echo "Compile universal binaries for xxHash"
cd external/xxHash
git apply ./../../xxHash.diff
cd  cmake_unofficial
cmake . -DBUILD_SHARED_LIBS=OFF -DXXHASH_BUILD_XXHSUM=OFF -DCMAKE_OSX_ARCHITECTURES="x86_64" -DDISPATCH=1 -B xxhash-x86_64
cmake --build xxhash-x86_64
cmake . -DBUILD_SHARED_LIBS=OFF -DXXHASH_BUILD_XXHSUM=OFF -DCMAKE_OSX_ARCHITECTURES="arm64" -B xxhash-arm64
cmake --build xxhash-arm64
for f in xxhash-arm64/*.a
do
    filename=$(basename $f)
    lipo -create -output ./../$filename xxhash-arm64/$filename xxhash-x86_64/$filename
done
cd ../../..

BOOSTROOT=./build/fat-homebrew-lib/boost cmake -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" -DMACOS_UNIVERSAL_BUILD=ON --preset $generator