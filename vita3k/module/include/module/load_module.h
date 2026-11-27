// Vita3K emulator project
// Copyright (C) 2026 Vita3K team
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

#include <emuenv/state.h>
#include <kernel/types.h>

#include <array>
#include <span>
#include <string>

// The flags of a module in the sysmodule table
#define SYSMODULE_MODULE_VS0 0x1
#define SYSMODULE_MODULE_DEVELOPMENT 0x4
#define SYSMODULE_MODULE_NOT_DEVELOPMENT 0x8
#define SYSMODULE_MODULE_SHARED 0x10
#define SYSMODULE_MODULE_APP0 0x20
#define SYSMODULE_MODULE_NONE 0x40
#define SYSMODULE_MODULE_DIPSW_D4 0x80
#define SYSMODULE_MODULE_DIPSW_C2 0x100

// The flags of a sysmodule id, what the program needs to use it
#define SYSMODULE_ID_GAME 0x1
#define SYSMODULE_ID_INTERNAL 0x2
#define SYSMODULE_ID_NON_GAME 0x4
#define SYSMODULE_ID_PSM 0x8
#define SYSMODULE_ID_SYSTEM 0x10
#define SYSMODULE_ID_DEVELOPMENT 0x20
#define SYSMODULE_ID_ARGS 0x40

// A module of the sysmodule table, its file name without .suprx and its SYSMODULE_MODULE_* flags
struct SysmoduleModule {
    const char *name;
    uint16_t flags;
};

// A sysmodule id, its SYSMODULE_ID_* flags and its modules as indexes into the module table. They
// load from the last one to the first.
struct SysmoduleId {
    uint16_t flags;
    std::span<const uint8_t> modules;
};

using SysmoduleModules = std::array<SysmoduleModule, SYSMODULE_MODULE_COUNT>;
using SysmoduleIds = std::array<SysmoduleId, SYSMODULE_COUNT>;
using SysmoduleInternalIds = std::array<SysmoduleId, SYSMODULE_INTERNAL_COUNT>;

extern const SysmoduleModules sysmodule_modules;
extern const SysmoduleIds sysmodule_ids;
extern const SysmoduleInternalIds sysmodule_internal_ids;

bool is_lle_module(SceSysmoduleModuleId module_id, EmuEnvState &emuenv);
bool is_lle_module(const std::string &module_name, EmuEnvState &emuenv);
