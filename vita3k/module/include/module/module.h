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

#include "bridge.h"

#include <util/log.h>

int unimplemented_impl(const char *name);
#define UNIMPLEMENTED() unimplemented_impl(export_name)

int stubbed_impl(const char *name, const char *info);
#define STUBBED(info) stubbed_impl(export_name, info)

#define BRIDGE_DECL(name) extern const ImportFn import_##name;
#define BRIDGE_IMPL(name) const ImportFn import_##name = bridge(&export_##name, #name);

#define EXPORT(ret, name, ...) ret export_##name(HostState &host, SceUID thread_id, const char *export_name, ##__VA_ARGS__)

#define VAR_BRIDGE_DECL(name) extern const ImportVarFactory import_##name;
#define VAR_BRIDGE_IMPL(name) const ImportVarFactory import_##name = export_##name;

#define VAR_EXPORT(name) Address export_##name(HostState &host)
