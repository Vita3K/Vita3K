/**
 * @file vita3k/dir_doc.cpp
 *
 * @brief Folder structure description
 *
 * This `.cpp` file contains descriptions for the `vita3k` source code folder and its sub-folders.
 */

/**
 * @dir vita3k
 *
 * @brief Source code files specific to the Vita3K emulator
 *
 * This folder is divided into different sub-folders that cover specific part/features of the emulatior.
 */

/**
 * @dir vita3k/app
 *
 * @brief GUI window and graphics rendering backend initiation and management
 */

/**
 * @dir vita3k/audio
 *
 * @brief Audio stream management
 */

/**
 * @dir vita3k/codec
 *
 * @brief Multimedia decodification and video/audio player for games
 */

/**
 * @dir vita3k/config
 *
 * @brief YAML configuration file management and CLI argument parsing
 */

/**
 * @dir vita3k/cpu
 *
 * @brief CPU emulation
 *
 * The PlayStation Vita has an ARMv7 CPU, making game binaries impossible to run on a x86 system unless recompiled.
 * Vita3K uses the Unicorn JIT recompiler to turn the Vita's ARM code into x86 code in order to enable execution of games in PCs.
 */

/**
 * @dir vita3k/crypto
 *
 * @brief Cryptographic tooling
 */

/**
 * @dir vita3k/ctrl
 *
 * @brief Gamepad input from host to PS Vita controls bridge
 */

/**
 * @dir vita3k/dialog
 *
 * @brief PS Vita OS dialog handling
 *
 * This folder contains the code necessary to handle the calls from the game to create native PS Vita OS dialogs.
 */

/**
 * @dir vita3k/disasm
 *
 * @brief ARM game code disassembler (used for the "Watch code" debugging option)
 */

/**
 * @dir vita3k/features
 *
 * @brief Host setup check for emulation features and optimizations
 */

/**
 * @dir vita3k/gdbstub
 *
 * @brief GDB integration
 */

/**
 * @dir vita3k/glutil
 *
 * @brief OpenGL shading language implementation
 */

/**
 * @dir vita3k/glutil
 *
 * @brief OpenGL shading language implementation
 */

/**
 * @dir vita3k/gui
 *
 * @brief UI elements management code
 */

/**
 * @dir vita3k/gxm
 *
 * @brief Vita's GXM low-level graphics API translation layer
 */

/**
 * @dir vita3k/host
 *
 * @brief Essential code needed to emulate the PS Vita OS
 */

/**
 * @dir vita3k/io
 *
 * @brief PS Vita file system emulation and `SceIo` implementation
 */

/**
 * @dir vita3k/kernel
 *
 * @brief PS Vita kernel and thread emulation
 */

/**
 * @dir vita3k/mem
 *
 * @brief Memory management
 */

/**
 * @dir vita3k/module
 *
 * @brief PS Vita module (library) loading and ARM calling convention to C++ HLE call bridge
 */

/**
 * @dir vita3k/modules
 *
 * @brief PS Vita SDK/OS call implementations
 *
 * This folder contains the C++ HLE equivalent implementations to the PlayStation Vita's SDK functions and methods, which
 * translate calls to the Vita SDK from games into host OS calls.
 *
 * All the SDK subsets, methods, functions, constants and variables are described in the [VitaSDK API](https://docs.vitasdk.org/index.html).
 */

/**
 * @dir vita3k/net
 *
 * @brief Ground code for `SceNet`
 */

/**
 * @dir vita3k/ngs
 *
 * @brief Implementation of Sony's NGS propietary audio library for the PlayStation Vita
 */

/**
 * @dir vita3k/nids
 *
 * @brief NID handling for PS Vita functions
 */

/**
 * @dir vita3k/np
 *
 * @brief PS Vita `Np` trophy API
 */

/**
 * @dir vita3k/renderer
 *
 * @brief OpenGL and Vulkan implementations
 */

/**
 * @dir vita3k/rtc
 *
 * @brief Time and clock implementation
 */

/**
 * @dir vita3k/rtc
 *
 * @brief Time and clock implementation
 */

/**
 * @dir vita3k/shader
 *
 * @brief GXM shader binary recompiler
 */

/**
 * @dir vita3k/threads
 *
 * @brief Thread management
 */

/**
 * @dir vita3k/touch
 *
 * @brief PS Vita touchscreen emulation
 */

/**
 * @dir vita3k/util
 *
 * @brief Several code utils used throughout the source code
 */
