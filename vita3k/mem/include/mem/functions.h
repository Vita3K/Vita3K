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

#include <mem/block.h>
#include <mem/util.h>

#include <functional>

struct MemState;

typedef std::function<bool(uint8_t *addr, bool write)> AccessViolationHandler;

constexpr Address user_main_memory_start = 0x80000000U;

// Permission when protecting a memory range
// Note: WriteOnly is actually not supported (ReadWrite used instead)
enum struct MemPerm {
    None = 0,
    ReadOnly = 1 << 0,
    WriteOnly = 1 << 1,
    ReadWrite = ReadOnly | WriteOnly
};

bool init(MemState &state, const bool use_page_table);
Address alloc(MemState &state, uint32_t size, const char *name, Address start_addr = user_main_memory_start);
Address alloc_aligned(MemState &state, uint32_t size, const char *name, unsigned int alignment, Address start_addr = user_main_memory_start);
void protect_inner(MemState &state, Address addr, uint32_t size, const MemPerm perm);
void unprotect_inner(MemState &state, Address addr, uint32_t size);
bool add_protect(MemState &state, Address addr, const uint32_t size, const MemPerm perm, const ProtectCallback &callback);
void open_access_parent_protect_segment(MemState &state, Address addr);
void close_access_parent_protect_segment(MemState &state, Address addr);
void add_external_mapping(MemState &mem, Address addr, uint32_t size, uint8_t *addr_ptr);
void remove_external_mapping(MemState &mem, uint8_t *addr_ptr, uint32_t size);
bool is_protecting(MemState &state, Address addr, MemPerm *perm = nullptr);
bool is_valid_addr(const MemState &state, Address addr);
bool is_valid_addr_range(const MemState &state, Address start, Address end);
bool handle_access_violation(MemState &state, uint8_t *addr, bool write) noexcept;
Block alloc_block(MemState &mem, uint32_t size, const char *name, Address start_addr = user_main_memory_start);
Address alloc_at(MemState &state, Address address, uint32_t size, const char *name);
Address try_alloc_at(MemState &state, Address address, uint32_t size, const char *name);
void free(MemState &state, Address address);
uint32_t mem_available(MemState &state);
const char *mem_name(Address address, MemState &state);
