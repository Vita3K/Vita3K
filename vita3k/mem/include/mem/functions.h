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

#include <utility>
#include <vector>

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

constexpr MemPerm most_restrictive_perm(MemPerm a, MemPerm b) {
    if (a == MemPerm::None || b == MemPerm::None)
        return MemPerm::None;
    if (a == MemPerm::ReadOnly || b == MemPerm::ReadOnly)
        return MemPerm::ReadOnly;
    if (a == MemPerm::WriteOnly || b == MemPerm::WriteOnly)
        return MemPerm::WriteOnly;
    return MemPerm::ReadWrite;
}

bool init(MemState &state, const bool use_page_table);
// Kubridge mappings share backing; they never copy destination data to source.
bool map_guest_memory(MemState &mem, Address dest, uint32_t size, Address source, MemPerm perm);
bool commit_guest_memory(MemState &mem, Address dest, uint32_t size, MemPerm perm);
bool protect_guest_memory(MemState &mem, Address dest, uint32_t size, MemPerm perm);
bool decommit_guest_memory(MemState &mem, Address dest, uint32_t size);
void release_guest_mappings(MemState &mem, Address dest, uint32_t size);
uint8_t *resolve_guest_pointer(const MemState &mem, Address addr);
// Guest ranges whose cached translations a write to [addr, addr + size) can change: the range itself
// plus every alias sharing backing with one of its pages. Pages only committed or protected count once.
std::vector<std::pair<Address, uint32_t>> guest_alias_ranges(const MemState &mem, Address addr, uint32_t size);
// Recompute guest_page_table entries after the native page table changed (external mappings).
void refresh_guest_page_table(MemState &mem, Address addr, uint32_t size);
bool access_guest_memory(MemState &mem, Address addr, void *data, uint32_t size, bool write, Address &fault);
bool check_guest_memory_access(MemState &mem, Address addr, uint32_t size, bool write, Address &fault);
bool compare_exchange_guest_memory(MemState &mem, Address addr, uint32_t size, uint64_t value, uint64_t expected, bool &swapped, Address &fault);
void deinit_mem(MemState &state);
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
