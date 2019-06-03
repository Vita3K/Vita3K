# Vita3K
[![Build Status](https://travis-ci.org/Vita3K/Vita3K.svg?branch=master)](https://travis-ci.org/Vita3K/Vita3K)
[![Build status](https://ci.appveyor.com/api/projects/status/tlvkwrsj13vq3gor/branch/master?svg=true)](https://ci.appveyor.com/project/Vita3K/vita3k/branch/master)
## Introduction
Vita3K is an experimental PlayStation Vita emulator for Windows, Linux and macOS.

* [Website](https://vita3k.org/) (information for users)
* [Wiki](https://github.com/Vita3K/Vita3K/wiki) (information for developers)
* [**Discord**](https://discord.gg/MaWhJVH) (recommended)
* IRC `#vita3k` on **freenode** ([Web-based IRC client](https://webchat.freenode.net/?channels=%23vita3k))
* [Patreon](https://www.patreon.com/Vita3K)

## Compatibility
The emulator currently runs some homebrew programs. It is also able to load some decrypted commercial games.

- [Homebrew compatibility database](https://github.com/Vita3K/homebrew-compatibility/issues)
- [Commerical compatiblity database](https://github.com/Vita3K/compatibility/issues)

[Alone with You](https://www.playstation.com/en-us/games/alone-with-you-psvita/) by **Benjamin Rivers**

![Alone with You screenshot](https://user-images.githubusercontent.com/20528385/57988943-1e955e80-7a62-11e9-8aa8-e96eacef8e60.png)

[VA-11 HALL-A](https://www.playstation.com/en-us/games/va-11-hall-a-psvita/) by **Sukeban Games**

![VA-11 HALL-A screenshot](https://user-images.githubusercontent.com/20528385/57989089-fad31800-7a63-11e9-85de-017b29d5cc15.png)

[VitaSnake](https://github.com/Grzybojad/vitaSnake/releases) by **Grzybojad**

![VitaSnake screenshot](https://user-images.githubusercontent.com/20528385/58014428-cd6b8600-7ac6-11e9-83ab-a3d2b79417fe.png)

## Licence
Vita3K is licensed under the **GPLv2** license. This is largely dictated by external dependencies, most notably Unicorn.

## Binaries

Vita3K binaries for Windows can be downloaded on [AppVeyor](https://ci.appveyor.com/project/Vita3K/vita3k/branch/master/artifacts).

## Building
### Prerequisites
[CMake](https://cmake.org/) is used to generate Visual Studio and Xcode project files. It is assumed that you have CMake installed and on your path. Other dependencies are provided as Git submodules or as prebuilt binaries.

After cloning, open a command prompt in Vita3K's directory and run:

`git submodule update --init --recursive`

to get all submodules.

### Windows (Visual Studio)
1. Run `gen-windows.bat` to create a `build-windows` directory and generate a Visual Studio solution in there.
2. Open the `Vita3K.sln` solution.
3. Set the startup project to `emulator`.
4. Build.

### macOS (Xcode)
1. Run `gen-macos.sh` to create a `build-macos` directory and generate an Xcode project in there.
2. Open the `Vita3K.xcodeproj` project.
3. When prompted to create schemes, create one for the `emulator` target only. The project builds many targets, so it will make your life easier if you create schemes as needed.
4. Build.

### Linux
1. Get [SDL](https://wiki.libsdl.org/Installation#Linux.2FUnix) (2.0.7+)
2. `git submodule init && git submodule update`
3. `gen-linux.sh`
4. `cd build-linux`
5. `make`, or `make -jN` where "N" is the amount of cores in your system.
##### Note: If Unicorn can't find Python, use `make UNICORN_QEMU_FLAGS="--python=/usr/bin/python2"` or similar to point it to your installation. `make` works after you do this once.

## Running
Specify the path to a .vpk file as the first command line argument, or run `Vita3K --help` from the command-line for a full list of options.
For more detailed instructions on running/installing games on all platforms, please read the **#info-faq** channel on our [Discord server](https://discord.gg/MaWhJVH).

## Bugs and issues
The project is at an early stage, so please be sensitive to that when opening new issues. Expect crashes, glitches, low compatibility and poor performance.

## Thanks
Thanks go out to people who offered advice or otherwise made this project possible, such as Davee, korruptor, Rinnegatamante, ScHlAuChi, Simon Kilroy, TheFlow, xerpi, xyz, Yifan Lu and many others.

## Donations
If you would like to show your appreciation or even help fund development, the project has a [Patreon](https://www.patreon.com/Vita3K) page.

## Supporters
Thank you to the following supporters:
* Mored1984

If you support us on Patreon and would like your name added, please get in touch or open a Pull Request.

## Note
The purpose of the emulator is not to enable illegal activity.

PlayStation and PS Vita are trademarks of Sony Interactive Entertainment Inc. The emulator is not related to or endorsed by Sony, or derived from confidential materials belonging to Sony.
