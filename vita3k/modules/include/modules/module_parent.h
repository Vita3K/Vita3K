// Vita3K emulator project
// Copyright (C) 2018 Vita3K team
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

#include <kernel/types.h>
#include <module/module.h>
#include <util/types.h>

struct CPUState;
struct HostState;
struct KernelState;
struct CPUDepInject;

void call_import(HostState &host, CPUState &cpu, uint32_t nid, SceUID thread_id);
bool load_module(HostState &host, SceSysmoduleModuleId module_id);
Address resolve_export(KernelState &kernel, uint32_t nid);
uint32_t resolve_nid(KernelState &kernel, Address addr);
std::string resolve_nid_name(KernelState &kernel, Address addr);

struct VarExport {
    uint32_t nid;
    ImportVarFactory factory;
    const char *name;
};

constexpr int var_exports_size =
#define NID(name, nid)
#define VAR_NID(name, nid) 1 +
#include <nids/nids.h>
    0;
#undef VAR_NID
#undef NID

const std::array<VarExport, var_exports_size> &get_var_exports();

CPUDepInject create_cpu_dep_inject(HostState &host);
