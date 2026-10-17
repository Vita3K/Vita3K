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
#include <util/types.h>

struct KernelState;
struct MemState;

Ptr<void> get_ipmi_client_vtable(KernelState &kernel, MemState &mem);
Ptr<void> get_shellsvc_client(KernelState &kernel, MemState &mem);
SceInt32 initialize_ipmi_client(KernelState &kernel, MemState &mem, Ptr<void> *client, Ptr<void> client_memory);