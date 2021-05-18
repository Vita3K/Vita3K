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

#include <mem/block.h>
#include <mem/util.h>

struct MemState;

bool init(MemState &state);
Address alloc(MemState &state, size_t size, const char *name);
Address alloc(MemState &state, size_t size, const char *name, unsigned int alignment);
Block alloc_block(MemState &mem, size_t size, const char *name);
Address alloc_at(MemState &state, Address address, size_t size, const char *name);
void free(MemState &state, Address address);
uint32_t mem_available(MemState &state);
const char *mem_name(Address address, MemState &state);
