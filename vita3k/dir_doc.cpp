/**
 * @file RPCSV/dir_doc.cpp
 *
 * @brief Folder structure description
 *
 * This `.cpp` file contains descriptions for the `RPCSV` source code folder and its sub-folders.
 */

/**
 * @dir RPCSV
 *
 * @brief Source code files specific to the RPCSV emulator
 *
 * This folder is divided into different sub-folders that cover specific part/features of the emulator.
 */

/**
 * @dir RPCSV/app
 *
 * @brief GUI window and graphics rendering backend initiation and management
 */

/**
 * @dir RPCSV/audio
 *
 * @brief Audio stream management
 */

/**
 * @dir RPCSV/codec
 *
 * @brief Multimedia decodification and video/audio player for games
 */

/**
 * @dir RPCSV/config
 *
 * @brief YAML configuration file management and CLI argument parsing
 */

/**
 * @dir RPCSV/cpu
 *
 * @brief CPU emulation
 *
 * The PlayStation Vita has an ARMv7 CPU, making game binaries impossible to run on a x86 system unless recompiled.
 * RPCSV uses the Unicorn JIT recompiler to turn the Vita's ARM code into x86 code in order to enable execution of games in PCs.
 */

/**
 * @dir RPCSV/crypto
 *
 * @brief Cryptographic tooling
 */

/**
 * @dir RPCSV/ctrl
 *
 * @brief Gamepad input from host to PS Vita controls bridge
 */

/**
 * @dir RPCSV/dialog
 *
 * @brief PS Vita OS dialog handling
 *
 * This folder contains the code necessary to handle the calls from the game to create native PS Vita OS dialogs.
 */

/**
 * @dir RPCSV/emuenv
 *
 * @brief Definitions and basic data structures to construct an emulated PS Vita environment
 */

/**
 * @dir RPCSV/disasm
 *
 * @brief ARM game code disassembler (used for the "Watch code" debugging option)
 */

/**
 * @dir RPCSV/features
 *
 * @brief Host setup check for emulation features and optimizations
 */

/**
 * @dir RPCSV/gdbstub
 *
 * @brief GDB integration
 */

/**
 * @dir RPCSV/glutil
 *
 * @brief OpenGL shading language implementation
 */

/**
 * @dir RPCSV/glutil
 *
 * @brief OpenGL shading language implementation
 */

/**
 * @dir RPCSV/gui
 *
 * @brief UI elements management code
 */

/**
 * @dir RPCSV/gxm
 *
 * @brief Vita's GXM low-level graphics API translation layer
 */

/**
 * @dir RPCSV/host
 *
 * @brief Host operating system abstraction layer
 */

/**
 * @dir RPCSV/io
 *
 * @brief PS Vita file system emulation and `SceIo` implementation
 */

/**
 * @dir RPCSV/kernel
 *
 * @brief PS Vita kernel and thread emulation
 */

/**
 * @dir RPCSV/mem
 *
 * @brief Memory management
 */

/**
 * @dir RPCSV/module
 *
 * @brief PS Vita module (library) loading and ARM calling convention to C++ HLE call bridge
 */

/**
 * @dir RPCSV/modules
 *
 * @brief PS Vita SDK/OS call implementations
 *
 * This folder contains the C++ HLE equivalent implementations to the PlayStation Vita's SDK functions and methods, which
 * translate calls to the Vita SDK from games into host OS calls.
 *
 * All the SDK subsets, methods, functions, constants and variables are described in the [VitaSDK API](https://docs.vitasdk.org/index.html).
 */

/**
 * @dir RPCSV/net
 *
 * @brief Ground code for `SceNet`
 */

/**
 * @dir RPCSV/ngs
 *
 * @brief Implementation of Sony's NGS propietary audio library for the PlayStation Vita
 */

/**
 * @dir RPCSV/nids
 *
 * @brief NID handling for PS Vita functions
 */

/**
 * @dir RPCSV/np
 *
 * @brief PS Vita `Np` trophy API
 */

/**
 * @dir RPCSV/packages
 *
 * @brief PS Vita software package handling
 */

/**
 * @dir RPCSV/renderer
 *
 * @brief OpenGL and Vulkan implementations
 */

/**
 * @dir RPCSV/rtc
 *
 * @brief Time and clock implementation
 */

/**
 * @dir RPCSV/rtc
 *
 * @brief Time and clock implementation
 */

/**
 * @dir RPCSV/shader
 *
 * @brief GXM shader binary recompiler
 */

/**
 * @dir RPCSV/threads
 *
 * @brief Thread management
 */

/**
 * @dir RPCSV/touch
 *
 * @brief PS Vita touchscreen emulation
 */

/**
 * @dir RPCSV/util
 *
 * @brief Several code utils used throughout the source code
 */
