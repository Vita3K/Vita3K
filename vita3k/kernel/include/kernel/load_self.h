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

#include <mem/ptr.h>
#include <util/fs.h>
#include <util/types.h>

#include <string>

struct KernelState;
struct MemState;
struct KernelModule;
struct ExportLibrary;

// The function NIDs of an export library and its entry table
struct ExportFunctions {
    const uint32_t *nids;
    Ptr<uint32_t> *entries;
    size_t count;
};

SceUID load_self(KernelState &kernel, MemState &mem, const void *self, const std::string &self_path, const fs::path &dump_path, uint16_t flags);
int unload_self(KernelState &kernel, MemState &mem, KernelModule &module);
// Links the module's imports and registers its export libraries in their order, up to the first
// that fails, which unlinks the imports again
int link_module(KernelState &kernel, MemState &mem, KernelModule &module);
// Unlinks the module's imports and unregisters its export libraries in their order, up to the
// first that fails, which links the imports again. A forced unlink takes the libraries even with
// strong imports linked to them and leaves the imports unlinked.
int unlink_module(KernelState &kernel, MemState &mem, KernelModule &module, bool forced);
ExportFunctions export_functions(const MemState &mem, const ExportLibrary &library);
// The address of an export of the module, 0 if it exports no such NID
Address find_module_export(const MemState &mem, const KernelModule &module, uint32_t nid);
// The NID in a module's module info
uint32_t module_nid(const MemState &mem, const KernelModule &module);
