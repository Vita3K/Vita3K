# Build Vita3K

Vita3K uses [CMake](https://cmake.org/) for its project configuration and generation should in theory be compatible with any project generator supported by CMake, C++17 compatible compiler and IDE with CMake support.

## External dependencies

Vita3K requires the following external dependencies to be pre-built and available to CMake when configuring the project:
- **Boost:** It can be satisfied by downloading a pre-built binary and ensuring the location can be found by CMake by either extracting/installing it in one of 
the locations included by default in [`CMAKE_SYSTEM_PREFIX_PATH`](https://cmake.org/cmake/help/latest/variable/CMAKE_SYSTEM_PREFIX_PATH.html), extracting it in 
`./.deps/<package_name>`, or extracting it in any other location given as an item of the [`CMAKE_PREFIX_PATH` CMake variable list](https://cmake.org/cmake/help/latest/envvar/CMAKE_PREFIX_PATH.html), as an item of the [`CMAKE_PREFIX_PATH` CMake environment variable list](https://cmake.org/cmake/help/latest/envvar/CMAKE_PREFIX_PATH.html) or as the value of the `BOOST_DIR` CMake variable when calling CMake to configure the project.

The CMake project also adds some additional directories to `CMAKE_PREFIX_PATH`. With these added, CMake will search for the dependencies with the following priority:
1. `${CMAKE_BINARY_DIR}/.deps`
2. Directories added by the user
3. `${CMAKE_CURRENT_SOURCE_DIR}/.deps`

This is done in order to aid cross-compilation scenarios, where one might use more general directories like `${CMAKE_CURRENT_SOURCE_DIR}/.deps` for native builds, but might want to eventually build for another platform while keeping the development setup for native builds. 

By populating `${CMAKE_BINARY_DIR}/.deps`, CMake will prefer dependencies from this directory even if any other directory listed in `CMAKE_PREFIX_PATH` has them available. Since CMake binary directories are dependent on the configuration used to generate them, they are the perfect place to
set up binaries targeting a foreign platform.

### Using Conan

Since not all dependencies are distributed as pre-built packages, Vita3K has a `conanfile.py` file that defines the dependencies it needs, as well as the configuration options for them so that the [Conan package manager](https://conan.io/) can be used to satisfy them regardless of the platform. In order to make the dependencies available to CMake, you will need to first install the dependencies in a directory (the Conan output folder) that will then be used by CMake to look for everything it needs.

```bash
conan install . -b missing -of .deps -pr:h <host_profile> -pr:b <build_profile>
```

- The **output folder (`-of`)** is the folder where Conan will put all the configuration files, and the Vita3K CMake project is set up to read the `./.deps` directory by default, as well as any directory listed in the [`CMAKE_PREFIX_PATH` CMake variable list](https://cmake.org/cmake/help/latest/envvar/CMAKE_PREFIX_PATH.html). Something to keep in mind is that any directory you add to the list will be preferred over the `./.deps` directory.

  In cross-compilation scenarios, it is also recommended to set the output folder to `<cmake_build_folder>/.deps`. The Vita3K CMake project is set up to look this directory for dependencies and if available, prefer dependencies provided in this directory before any other.

- The **[profile file (`-pr`)](https://docs.conan.io/2/reference/config_files/profiles.html)** is a file that tells Conan which options (ex. compiler, build type...) you want to use to build the dependencies pointed at the `conanfile.py`. However, this can also be specified and even overwritten in the command line by using certain arguments.
  
  Conan requires a profile to be specified for both the host (target platform) and build (platform used to compile) platform.
  - When building a **native** executable (host and build platform are the same), the same profile
  file is used as the value of the host and build profile arguments. However, this is tedious and
  can be omitted by creating a `default` profile by calling `conan profile detect` and then editing
  the generated profile to match the toolchain which is going to be used.
  After doing this, Conan can be invoked without the profile arguments:
    ```bash
    conan install . -b missing -of .deps
    ```
    Adding any other profile to the very same location will also allow you to use that profile just by using its file name as the value for the `-pr:<type>` argument. You can do so without having to copy it yourself by running:
    ```bash
    conan config install -tf profiles <path_to_profile>
    ```
  - When **cross-building**, two different build and host profiles would need to be written and passed to Conan.
  [Please refer to the Conan documentation for more info.](https://docs.conan.io/2/tutorial/consuming_packages/cross_building_with_conan.html).

To make it easier to use Conan and prevent errors with certain dependencies, this project also supplies pre-configured Conan profiles that match a combination of target OS and compiler and toolchain (if possible or necessary) located in `conan/profiles/`. **These however should only serve
as a reference to write your own profile matching your specific platform and toolchain used to compile Vita3K.**

| Included Conan profile | Compiler / Toolchain | C++ library |
| --- | --- | --- |
| `linux-clang_libstdc++` | Clang | GNU `libstdc++` |
| `linux-gcc` | GCC | GNU `libstdc++` |
| `windows-msvc` | Microsoft Visual C/C++ Compiler (Visual Studio 2022) | Microsoft C/C++ runtime and Microsoft C++ STL |
| `windows-msvc_llvm` | Clang-CL (Visual Studio 2022) | Microsoft C/C++ runtime and Microsoft C++ STL |
| `macos-clang` | Apple Clang | LLVM `libc++` |

In order to save trouble with CI configuration, the Conan profile files provided by Vita3K are set to build all the dependencies in `Release` mode, which can cause linking issues in development environments, specially on Windows. If your goal is to do Vita3K development, it is recommended to run as many Conan install as build types you use to ensure all dependencies will link regardless of the build type:
```bash
# Default build type specified in profile (Release)
conan install . -b missing -of .deps -pr:h <host_profile> -pr:b <build_profile>

# Debug
conan install . -b missing -of .deps -pr:h <host_profile> -pr:b <build_profile> -s build_type=Debug

# RelWithDebInfo
conan install . -b missing -of .deps -pr:h <host_profile> -pr:b <build_profile> -s build_type=RelWithDebInfo
```

## CMake configuration
The project provides [CMake presets](https://cmake.org/cmake/help/latest/manual/cmake-presets.7.html) to allow configuring and building Vita3K without having to deal with adding the needed arguments through a command-line or using the user interface of your IDE or code editor of choice. As long as your IDE or code editor supports CMake, the software should immediately detect the presets and let you choose which configure settings you want to use to generate the project. Reference on how to use CMake presets with various IDEs and code editors can be found here:

- [Visual Studio](https://docs.microsoft.com/en-us/cpp/build/cmake-presets-vs)
- [Visual Studio Code](https://github.com/microsoft/vscode-cmake-tools/blob/main/docs/cmake-presets.md)
- [CLion](https://www.jetbrains.com/help/clion/cmake-presets.html): CMake presets in CLion are imported as CLion's [CMake profiles](https://www.jetbrains.com/help/clion/cmake-profile.html).

All presets are named after `<target_os>-<project_generator>-<compiler>`, are automatically hidden and shown depending on your host OS and generate a binary folder of path `<source_directory>/build/<preset_name>`. For command-line users, run `cmake --list-presets` on the top directory of the repository to see which presets are available to you. For presets without `<project_generator>` and/or `<compiler>`, the project generator and/or the compiler haven't been explicitly specified in the preset to let CMake fallback to the platform defaults.

If you still want to use presets but none of them works for your setup, you can make new ones by creating a `CMakeUserPresets.json` file and you can check the specification [here](https://cmake.org/cmake/help/latest/manual/cmake-presets.7.html). Git will ignore this file.

**Note: Vita3K doesn't support compilation for 32-bit/x86/i386 platforms.**

For convenience, the following building instructions are given as examples:

## Windows

### Visual Studio 2022
1. Install Visual Studio 2022 and choose to install `Desktop development with C++`. You will get compiler and `cmake` required for building.

    Example for Visual Studio 2019:

    ![Required tools for VS 2022](https://i.imgur.com/bkY15Oh.png)

2. Install `git` to `clone` the project. Download and install `git` from [here](https://git-scm.com).

3. Clone this repo.
    ```cmd
    git clone --recursive https://github.com/Vita3K/Vita3K
    cd Vita3K
    ```

4. [Satisfy the required external dependencies if you haven't already](#external-dependencies). Example for doing so with Conan (you only need to run the last two if you plan on doing development or debugging):
    
    **Note:** You might need to do this in a Visual Studio Developer Command Prompt like `x64 Native Tools Command Prompt for VS 2022`.
    ```cmd
    conan install . -b missing -of .deps -pr:h conan\profiles\windows-msvc -pr:b conan\profiles\windows-msvc 
    conan install . -b missing -of .deps -pr:h conan\profiles\windows-msvc -pr:b conan\profiles\windows-msvc -s build_type=Debug
    conan install . -b missing -of .deps -pr:h conan\profiles\windows-msvc -pr:b conan\profiles\windows-msvc -s build_type=RelWithDebInfo
    ```

5. Run Visual Studio 2022. On the project selection window open the local clone of the repository as a folder. Thanks to the integration between Visual Studio and CMake, Visual Studio will automatically detect the repository as a CMake project.
6. Wait for all components for Visual Studio to be loaded. At the top of the window, there should be three new menus that allow you to select the target (specific to Visual Studio), a CMake configure preset and a CMake build preset (if available). Recommended preset for Visual Studio 2022 is *Windows with Visual Studio 2022*.

From there, the project will be ready to build right from the Visual Studio UI.

If you aren't satisfied with the way the Visual Studio integrates CMake projects and you would like to just use regular Visual Studio solution files (`.sln`), you can close Visual Studio and then open the solution file found in `build/<preset_name>/`.


### Build using terminal
1. Install:
    -  [Git](https://git-scm.com)
    -  [CMake](https://cmake.org/download/)
    -  Either the [Build Tools for Visual Studio 2022](https://aka.ms/vs/17/release/vs_BuildTools.exe) or Visual Studio 2022 with the ` Desktop development with C++` workload.
    - [Python](https://www.python.org/downloads/windows/) and [Conan](https://conan.io/downloads.html).

2. On the Start Menu, open the `x64 Native Tools Command Prompt for VS 2022`.
    <p align="center">
      <img src="./_building/vs-cmd-prompt.png">
    </p>

3. Clone the repository:
    ```cmd
    git clone --recursive https://github.com/Vita3K/Vita3K
    cd Vita3K
    ```

4. [Satisfy the required external dependencies if you haven't already](#external-dependencies). Example for doing so with Conan (you only need to run the last two if you plan on doing development or debugging):
    ```cmd
    conan install . -b missing -of .deps -pr:h conan\profiles\windows-msvc -pr:b conan\profiles\windows-msvc 
    conan install . -b missing -of .deps -pr:h conan\profiles\windows-msvc -pr:b conan\profiles\windows-msvc -s build_type=Debug
    conan install . -b missing -of .deps -pr:h conan\profiles\windows-msvc -pr:b conan\profiles\windows-msvc -s build_type=RelWithDebInfo
    ```

5. Generate the project:
    ```cmd
    cmake --preset windows-vs2022
    ```
    The line above will generate a Visual Studio 2022 project inside a folder called `build/windows-vs2022`.

6. Build the project:
    ```cmd
    cmake --build build/windows-vs2022
    ```

## macOS (Xcode)

1. Install Xcode at App Store.

2. Install [`brew`](https://brew.sh).

3. Install dependencies with `brew`.
    ```sh
    brew install git cmake python molten-vk openssl
    ```

4. Clone this repo.
    ```sh
    git clone --recursive https://github.com/Vita3K/Vita3K
    cd Vita3K
    ```

5. [Satisfy the required external dependencies if you haven't already](#external-dependencies). Example for doing so with Conan (you only need to run the last two if you plan on doing development or debugging):
    ```cmd
    conan install . -b missing -of .deps -pr:h conan\profiles\macos-clang -pr:b conan\profiles\macos-clang
    conan install . -b missing -of .deps -pr:h conan\profiles\macos-clang -pr:b conan\profiles\macos-clang -s build_type=Debug
    conan install . -b missing -of .deps -pr:h conan\profiles\macos-clang -pr:b conan\profiles\macos-clang -s build_type=RelWithDebInfo
    ```

6. Generate Xcode project.
    ```
    cmake --preset macos-xcode
    ```
    This example will generate a Xcode project inside a folder called `build/macos-xcode`.

7. Open Xcode project `vita3k.xcodeproj` generated in `build/macos-xcode` directory.

8. When prompted to create schemes, create one for the `vita3k` target only. The project builds many targets, so it will make your life easier if you create schemes as needed.

9. Build the project using the Xcode UI. If needed, the build process can be invoked as well the same way as with the other platforms using a terminal:
    ```sh
    cmake --build build/macos-xcode --config Release
    ```

## Linux

In Linux, Conan will try to automatically install any additional system packages requested by the Conan packages that need to be built. However, Conan may also ask you to install some additional system packages manually.

### Ubuntu/Debian

**Note:** The CMake preset `linux-ninja-clang` makes use of the LLD linker, which will need to be installed in your system along with Clang.

1. Install dependencies.
    ```sh
    sudo apt install git cmake ninja-build libsdl2-dev pkg-config libgtk-3-dev clang lld xdg-desktop-portal openssl libssl-dev
    pip install conan
    ```

2. Clone this repo.
    ```sh
    git clone --recursive https://github.com/Vita3K/Vita3K
    cd Vita3K
    ```

3. [Satisfy the required external dependencies if you haven't already](#external-dependencies). Example for doing so with Conan (you only need to run the last two if you plan on doing development or debugging):
    ```cmd
    conan install . -b missing -of .deps -pr:h conan\profiles\linux-clang_libstdc++ -pr:b conan\profiles\linux-clang_libstdc++
    conan install . -b missing -of .deps -pr:h conan\profiles\linux-clang_libstdc++ -pr:b conan\profiles\linux-clang_libstdc++ -s build_type=Debug
    conan install . -b missing -of .deps -pr:h conan\profiles\linux-clang_libstdc++ -pr:b conan\profiles\linux-clang_libstdc++ -s build_type=RelWithDebInfo
    ```

4. Generate the project.
    ```sh
    cmake --preset linux-ninja-clang
    ```
    This example will generate a Ninja Multi-Config (`ninja-build`) project instead of a Make (`make`, the default project generator for Linux) one inside the folder `build/linux-ninja-clang`.

5. Build the project:
    ```sh
    cmake --build build/linux-ninja-clang --config Release
    ```

## Note

- After cloning or checking out a branch, you should always update submodules.
  ```sh
  git submodule update --init --recursive
  ```
