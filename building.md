# Build Vita3K

## Windows (Visual Studio)

- Install Visual Studio 2017 or 2019 and choose to install `Desktop development with C++`. You will get the compiler and `cmake` required for building.

Example for Visual Studio 2019:

![Required tools for VS 2019](https://i.imgur.com/bkY15Oh.png)

- Install `git` to `clone` the project. Download and install `git` from [here](https://git-scm.com).

- Clone this repo.

```cmd
git clone --recursive https://github.com/Vita3K/Vita3K
cd Vita3K
```

- Open `Developer Command Prompt` for your respective Visual Studio version. `cmake` and other `C/C++` toolchain only available on `Developer Command Prompt`. 

Example for Visual Studio 2019:

![Developer Command Prompt for VS 2019](https://i.imgur.com/w6Umx1S.png)

- Run `gen-windows.bat` to create Visual Studio project in `build-windows` directory.

```cmd
.\gen-windows
```

- Open the project generated in the `build-windows` directory.

- Build and run it.

## macOS (Xcode)

- Install Xcode at App Store.

- Install [`brew`](https://brew.sh).

- Install dependencies with `brew`.

```sh
brew install git cmake
```

- Clone this repo.

```sh
git clone --recursive https://github.com/Vita3K/Vita3K
cd Vita3K
```

- Generate Xcode project.

```
./gen-macos.sh
```

- Open Xcode project `vita3k.xcodeproj` generated in `build-macos` directory.

- When prompted to create schemes, create one for the `vita3k` target only. The project builds many targets, so it will make your life easier if you create schemes as needed.

- Build.

## Linux

### Ubuntu/Debian

- Install dependencies.

```sh
sudo apt install git cmake ninja-build libsdl2-dev pkg-config libgtk-3-dev clang
```

- Clone this repo.

```sh
git clone --recursive https://github.com/Vita3K/Vita3K
cd Vita3K
```

- Build the project.

```sh
./gen-linux.sh
cd build-linux
ninja
```

## Note

- After cloning or checking out a branch, you should always update submodules.

`git submodule update --init --recursive`

- If `boost` failed to build, you can opt out for system `boost` package (Linux and macOS only).

```sh
brew install boost # for macOS
sudo apt install libboost-all-dev # for Ubuntu/Debian
```
