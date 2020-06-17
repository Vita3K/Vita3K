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
#include <util/types.h>

template <class T>
class Ptr;
struct KernelState;
struct MemState;

Ptr<Ptr<void>> get_thread_tls_addr(KernelState &kernel, MemState &mem, SceUID thread_id, int key);
void stop_all_threads(KernelState &kernel);

void add_watch_memory_addr(KernelState &state, Address addr, size_t size);
void remove_watch_memory_addr(KernelState &state, Address addr);
bool is_watch_memory_addr(KernelState &state, Address addr);

void update_watches(KernelState &state);
