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
#include "bridge_args.h"
#include "bridge_return.h"

#include <host/import_fn.h>
#include <host/state.h>

#include <cassert>

int unimplemented(const char *name);

#define BRIDGE_DECL(name) extern ImportFn *const import_##name;
#define BRIDGE_IMPL(name) ImportFn *const import_##name = &Bridge<decltype(&export_##name), &export_##name>::call;

#define EXPORT(ret, name, ...) ret export_##name(HostState &host, SceUID thread_id, ##__VA_ARGS__)
