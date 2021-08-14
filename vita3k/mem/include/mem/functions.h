// Vita3K emulator project
// Copyright (C) 2021 Vita3K team
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

typedef std::function<bool(uint8_t *addr, bool write)> AccessViolationHandler;

bool init(MemState &state);
Address alloc(MemState &state, size_t size, const char *name);
Address alloc(MemState &state, size_t size, const char *name, unsigned int alignment);
bool add_write_protect(MemState &state, Address addr, const size_t size, WriteProtectCallback callback);
bool remove_write_protect(MemState &state, Address addr);
bool is_valid_addr(const MemState &state, Address addr);
bool is_valid_addr_range(const MemState &state, Address start, Address end);
bool handle_access_violation(MemState &state, uint8_t *addr, bool write) noexcept;
Block alloc_block(MemState &mem, size_t size, const char *name);
Address alloc_at(MemState &state, Address address, size_t size, const char *name);
void free(MemState &state, Address address);
uint32_t mem_available(MemState &state);
const char *mem_name(Address address, MemState &state);
