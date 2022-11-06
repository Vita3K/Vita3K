// Vita3K emulator project
// Copyright (C) 2022 Vita3K team
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License along
// with this program; if not, write to the Free Software Foundation, Inc.,
// 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

#pragma once

#include <config/functions.h> // for is_tracy_advanced_profiling_active_for_module
#include <mem/ptr.h>
#include <mem/state.h>
#include <sstream>
#include <util/log.h>

// universal to string converters for module specific types (usually enums)
template <typename T>
std::string to_debug_str(const MemState &mem, T data) {
    std::stringstream datass;
    datass << data;
    return datass.str();
}

// Override pointers, we want to print the address in hex
template <typename U>
std::string to_debug_str(const MemState &mem, U *data) {
    std::stringstream datass;
    datass << log_hex_fast(Ptr<U>(data, mem).address()); // Convert host ptr to guest
    return datass.str();
}

// Override for guest pointers, we want to print the guest address
template <typename U>
std::string to_debug_str(const MemState &mem, Ptr<U> data) {
    std::stringstream datass;
    datass << log_hex_fast(data.address());
    return datass.str();
}

template <>
inline std::string to_debug_str(const MemState &mem, Ptr<char> data) {
    return std::string(data.address() ? log_hex_fast(data.address()) + " " + data.get(mem) : "0x0 NULLPTR");
}

// Override for char pointers as the contents are readable
template <>
inline std::string to_debug_str(const MemState &mem, char *data) {
    // Format for correct char* should be "(address in hex (0x12345)) (string)", this is just in the
    // extreme case that the string is actually "0x0 NULLPTR" and be confusing
    return std::string(data ? log_hex_fast(Ptr<char *>(data, mem).address()) + " " + data : "0x0 NULLPTR");
}

// Override for char pointers as the contents are readable
template <>
inline std::string to_debug_str(const MemState &mem, const char *data) {
    return std::string(data ? log_hex_fast(Ptr<const char *>(data, mem).address()) + " " + data : "0x0 NULLPTR");
}

template <>
inline std::string to_debug_str<std::string>(const MemState &mem, std::string data) {
    return data;
}

inline std::string to_debug_str(const MemState &mem) {
    return "";
}

#ifdef TRACY_ENABLE

#include "Tracy/client/TracyScoped.hpp"
#include "Tracy/tracy/Tracy.hpp"

#if (defined(_MSC_VER) && !defined(__clang__) && (!defined(_MSVC_TRADITIONAL) || _MSVC_TRADITIONAL))
#define __ARGS_WITH_COMMA(...) , ##__VA_ARGS__
#define __TRACY_LOG_ARG_IF(arg)                                                                  \
    if constexpr ((#arg "")[0] != '\0') {                                                        \
        const std::string arg_str = #arg ": " + to_debug_str(emuenv.mem __ARGS_WITH_COMMA(arg)); \
        ___tracy_scoped_zone.Text(arg_str.c_str(), arg_str.size());                              \
    }
#else
#define __TRACY_LOG_ARG_IF3(arg)                                               \
    {                                                                          \
        const std::string arg_str = #arg ": " + to_debug_str(emuenv.mem, arg); \
        ___tracy_scoped_zone.Text(arg_str.c_str(), arg_str.size());            \
    }

#define __TRACY_LOG_ARG_IF2(dummy, ...) __VA_OPT__(__TRACY_LOG_ARG_IF3(__VA_ARGS__))

#define __TRACY_LOG_ARG_IF(arg) __TRACY_LOG_ARG_IF2(dummy, arg)
#endif

#define __TRACY_FUNC(module_name_var, name, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9, ...)                                                  \
    static_assert(std::basic_string_view(__FUNCTION__) == "export_" #name);                                                                             \
    bool _tracy_activation_state = config::is_tracy_advanced_profiling_active_for_module(emuenv.cfg.tracy_advanced_profiling_modules, module_name_var); \
    ZoneNamedN(___tracy_scoped_zone, #name, _tracy_activation_state);                                                                                   \
    if (_tracy_activation_state) {                                                                                                                      \
        __TRACY_LOG_ARG_IF(arg1)                                                                                                                        \
        __TRACY_LOG_ARG_IF(arg2)                                                                                                                        \
        __TRACY_LOG_ARG_IF(arg3)                                                                                                                        \
        __TRACY_LOG_ARG_IF(arg4)                                                                                                                        \
        __TRACY_LOG_ARG_IF(arg5)                                                                                                                        \
        __TRACY_LOG_ARG_IF(arg6)                                                                                                                        \
        __TRACY_LOG_ARG_IF(arg7)                                                                                                                        \
        __TRACY_LOG_ARG_IF(arg8)                                                                                                                        \
        __TRACY_LOG_ARG_IF(arg9)                                                                                                                        \
    }

#define TRACY_MODULE_NAME(module_name) const std::string tracy_module_name = #module_name;

const std::string tracy_renderer_command_name = "Renderer commands";

// TODO: Support more stuff for commands, like arguments
#define __TRACY_FUNC_COMMANDS(name)                                                                                                                             \
    bool _tracy_activation_state = config::is_tracy_advanced_profiling_active_for_module(config.tracy_advanced_profiling_modules, tracy_renderer_command_name); \
    ZoneNamedN(___tracy_scoped_zone, #name, _tracy_activation_state);

// TODO: Support more stuff for commands, like arguments
#define __TRACY_FUNC_COMMANDS_SET_STATE(name)                                                                                                                   \
    bool _tracy_activation_state = config::is_tracy_advanced_profiling_active_for_module(config.tracy_advanced_profiling_modules, tracy_renderer_command_name); \
    ZoneNamedN(___tracy_scoped_zone, #name, _tracy_activation_state);

// workaround for variadic macro in "traditional" MSVC preprocessor.
// https://docs.microsoft.com/en-us/cpp/preprocessor/preprocessor-experimental-overview?view=msvc-170#macro-arguments-are-unpacked
#if (defined(_MSC_VER) && !defined(__clang__) && (!defined(_MSVC_TRADITIONAL) || _MSVC_TRADITIONAL))
#pragma warning(disable : 4003) // This warning is SUPER annoying, shut the warning up c:
#define TRACY_FUNC(name, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8) __TRACY_FUNC(tracy_module_name, name, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8)
#define TRACY_FUNC_M(module_name_var, name, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8) __TRACY_FUNC(module_name_var, name, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8)
#define TRACY_FUNC_COMMANDS(name) __TRACY_FUNC_COMMANDS(name)
#define TRACY_FUNC_COMMANDS_SET_STATE(name) __TRACY_FUNC_COMMANDS_SET_STATE(name)
#else
#define TRACY_FUNC(name, ...) __TRACY_FUNC(tracy_module_name, name, ##__VA_ARGS__, , , , , , , , , )
#define TRACY_FUNC_M(module_name_var, name, ...) __TRACY_FUNC(module_name_var, name, ##__VA_ARGS__, , , , , , , , , )
#define TRACY_FUNC_COMMANDS(name) __TRACY_FUNC_COMMANDS(name)
#define TRACY_FUNC_COMMANDS_SET_STATE(name) __TRACY_FUNC_COMMANDS_SET_STATE(name)
#endif // "traditional" MSVC preprocessor
#else // if not TRACY_ENABLE
#define TRACY_FUNC(name, ...)
#define TRACY_FUNC_M(module_name_var, name, ...)
#define TRACY_MODULE_NAME(module_name)
#define TRACY_FUNC_COMMANDS(name)
#define TRACY_FUNC_COMMANDS_SET_STATE(name)
#endif // TRACY_ENABLE
