# Build Vita3K

Vita3K uses CMake for its project configuration and generation. In theory, it should be compatible with any project generator supported by CMake, C++17 compatible compiler and an IDE with CMake support. 

The project provides [CMake presets](https://cmake.org/cmake/help/latest/manual/cmake-presets.7.html) to allow configuring and building Vita3K without having to deal with adding the needed arguments through a command-line interface or using the user interface of your IDE. As long as your IDE or code editor supports CMake, the software should immediately detect the presets and let you choose which configuration settings you want to use to generate the program. Reference on how to use CMake presets with various IDEs and code editors can be found here:

- [Visual Studio](https://docs.microsoft.com/en-us/cpp/build/cmake-presets-vs)
- [Visual Studio Code](https://github.com/microsoft/vscode-cmake-tools/blob/main/docs/cmake-presets.md)
- [CLion](https://www.jetbrains.com/help/clion/cmake-presets.html): CMake presets in CLion are imported as CLion's [CMake profiles](https://www.jetbrains.com/help/clion/cmake-profile.html).

All presets are named after `<target_os>-<project_generator>-<compiler>`, are automatically hidden and shown depending on your host OS and generate a binary folder of path `<source_directory>/build/<preset_name>`. For command-line users, run `cmake --list-presets` on the top directory of the repository to see which presets are available to you. For presets without `<project_generator>` and/or `<compiler>`, the project generator and/or the compiler haven't been explicitly specified in the preset to let CMake fallback to the platform defaults.

If you still want to use presets but none of them works for your setup, you can make new ones by creating a `CMakeUserPresets.json` file and you can check the specification [here](https://cmake.org/cmake/help/latest/manual/cmake-presets.7.html). Git will ignore this file.

**Note: Vita3K doesn't support compilation for 32-bit/x86/i386 platforms.**

For convenience, the following building instructions are given as examples:

## Windows

### Visual Studio 2022
- Install Visual Studio 2022 and choose to install `Desktop development with C++`. You will get compiler and `cmake` required for building.

  Example for Visual Studio 2019:

  ![Required tools for VS 2022](https://i.imgur.com/bkY15Oh.png)

- Install `git` to `clone` the project. Download and install `git` from [here](https://git-scm.com).

- Clone this repo.

  ```cmd
  git clone --recursive https://github.com/Vita3K/Vita3K
  cd Vita3K
  ```

- Run Visual Studio 2022. On the project selection window open the local clone of the repository as a folder. Thanks to the integration between Visual Studio and CMake, Visual Studio will automatically detect the repository as a CMake project.
- Wait for all components for Visual Studio to be loaded. At the top of the window, there should be three new menus that allow you to select the target (specific to Visual Studio), a CMake configure preset and a CMake build preset (if available). Recommended preset for Visual Studio 2022 is *Windows with Visual Studio 2022*.

From there, the project will be ready to build right from the Visual Studio UI.

If you aren't satisfied with the way the Visual Studio integrates CMake projects and you would like to just use regular Visual Studio solution files (`.sln`), you can close Visual Studio and then open the solution file found in `build/<preset_name>/`.


### Build using terminal
-  Install:
   -  [Git](https://git-scm.com)
   -  [CMake](https://cmake.org/download/)
   -  Either the [Build Tools for Visual Studio 2022](https://aka.ms/vs/17/release/vs_BuildTools.exe) or Visual Studio 2022 with the ` Desktop development with C++` workload.
- On the Start Menu, open the `x64 Native Tools Command Prompt for Visual Studio 2022`.
  <p align="center">
    <img src="./_building/vs-cmd-prompt.png">
  </p>

- Clone the repository:
  ```cmd
  git clone --recursive https://github.com/Vita3K/Vita3K
  cd Vita3K
  ```

- Generate the project:
  ```cmd
  cmake --preset windows-vs2022
  ```
  The line above will generate a Visual Studio 2022 project inside a folder called `build/windows-vs2022`.

- Build the project:
  ```cmd
  cmake --build build/windows-vs2022
  ```

## macOS (Xcode)

- Install Xcode at App Store.

- Install [`brew`](https://brew.sh).

- Install dependencies with `brew`.

  ```sh
  brew install git cmake molten-vk openssl
  ```

- Clone this repo.

  ```sh
  git clone --recursive https://github.com/Vita3K/Vita3K
  cd Vita3K
  ```

- Generate Xcode project.

  ```
  cmake --preset macos-xcode
  ```
  This example will generate a Xcode project inside a folder called `build/macos-xcode`.

- Open Xcode project `vita3k.xcodeproj` generated in `build/macos-xcode` directory.

- When prompted to create schemes, create one for the `vita3k` target only. The project builds many targets, so it will make your life easier if you create schemes as needed.

- Build the project using the Xcode UI. If needed, the build process can be invoked as well the same way as with the other platforms using a terminal:
  ```sh
  cmake --build build/macos-xcode
  ```

## Linux

- Install dependencies.

### Ubuntu/Debian

  ```sh
  sudo apt install git cmake ninja-build libsdl2-dev pkg-config libgtk-3-dev clang lld xdg-desktop-portal openssl libssl-dev
  ```

### Fedora

  ```sh
  sudo dnf install git cmake ninja-build SDL2-devel pkg-config gtk3-devel clang lld xdg-desktop-portal openssl openssl-devel libstdc++-static
  ```

Note: The CMake preset `linux-ninja-clang` makes use of the LLD linker, which will need to be installed in your system along with Clang.

- Clone this repo.

  ```sh
  git clone --recursive https://github.com/Vita3K/Vita3K
  cd Vita3K
  ```

- Generate the project.

  ```sh
  cmake --preset linux-ninja-clang
  ```
  This example will generate a Ninja Multi-Config (`ninja-build`) project instead of a Make (`make`, the default project generator for Linux) one inside the folder `build/linux-ninja-clang`.

- Build the project:
  ```sh
  cmake --build build/linux-ninja-clang
  ```

## Note

- After cloning or checking out a branch, you should always update submodules.
  ```sh
  git submodule update --init --recursive
  ```

- If Boost failed to build, you can use the system Boost package (Linux and macOS only).

  ```sh
  brew install boost # for macOS
  sudo apt install libboost-filesystem-dev libboost-program-options-dev libboost-system-dev # for Ubuntu/Debian
  ```

  If needed, CMake options `VITA3K_FORCE_CUSTOM_BOOST` and `VITA3K_FORCE_SYSTEM_BOOST` can be set to change the way the CMake project looks for Boost.
