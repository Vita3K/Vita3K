// Vita3K emulator project
// Copyright (C) 2025 Vita3K team
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

#include "bridge.h"

#include <util/log.h>

#define UNIMPLEMENTED()                                                \
    ([&]() {                                                           \
        LOG_WARN_ONCE("Unimplemented {} import called.", export_name); \
        return 0;                                                      \
    })()

#define STUBBED(info)                                                       \
    ([&]() {                                                                \
        LOG_WARN_ONCE("Stubbed {} import called. ({})", export_name, info); \
        return 0;                                                           \
    })()

#define CALL_EXPORT(name, ...) export_##name(emuenv, thread_id, #name, ##__VA_ARGS__)

#define DECL_EXPORT(ret, name, ...) ret export_##name(EmuEnvState &emuenv, SceUID thread_id, const char *export_name, ##__VA_ARGS__)
#define EXPORT(ret, name, ...)                                           \
    DECL_EXPORT(ret, name, ##__VA_ARGS__);                               \
    extern const ImportFn import_##name = bridge(&export_##name, #name); \
    DECL_EXPORT(ret, name, ##__VA_ARGS__)

#define DECL_VAR_EXPORT(name) Address export_##name(EmuEnvState &emuenv)
#define VAR_EXPORT(name)                                         \
    DECL_VAR_EXPORT(name);                                       \
    extern const ImportVarFactory import_##name = export_##name; \
    DECL_VAR_EXPORT(name)

#define LIBRARY_INIT(name)                                                              \
    static void export_library_init_##name(EmuEnvState &emuenv);                        \
    extern const LibraryInitFn import_library_init_##name = export_library_init_##name; \
    static void export_library_init_##name(EmuEnvState &emuenv)
