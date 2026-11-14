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

#include <mem/block.h>
#include <mem/util.h>

#include <atomic>
#include <functional>
#include <memory>

struct MemState;

using AccessViolationHandler = std::function<bool(uint8_t *addr, bool write)>;

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
void deinit_mem(MemState &state);
Address alloc(MemState &state, uint32_t size, const char *name, Address start_addr = user_main_memory_start);
Address alloc_aligned(MemState &state, uint32_t size, const char *name, unsigned int alignment, Address start_addr = user_main_memory_start);
void protect_inner(MemState &state, Address addr, uint32_t size, const MemPerm perm);
void unprotect_inner(MemState &state, Address addr, uint32_t size);
bool add_protect(MemState &state, Address addr, const uint32_t size, const MemPerm perm, const ProtectCallback &callback);
// Software-cache notifications may be invalidated conservatively when opening
// a shared host page in the unmapped view (collateral notifications use write=true).
// Guest access/abort callbacks must keep using add_protect. Mapped-view handling
// remains unchanged because external page-table backing may be noncontiguous.
bool add_cache_protect(MemState &state, Address addr, uint32_t size, MemPerm perm, const ProtectCallback &callback);
void open_access_parent_protect_segment(MemState &state, Address addr);
void close_access_parent_protect_segment(MemState &state, Address addr);
void add_external_mapping(MemState &mem, Address addr, uint32_t size, uint8_t *addr_ptr);
void remove_external_mapping(MemState &mem, uint8_t *addr_ptr, uint32_t size);
bool is_protecting(MemState &state, Address addr, MemPerm *perm = nullptr);
bool is_valid_addr(const MemState &state, Address addr);
bool is_valid_addr_range(const MemState &state, Address start, Address end);
bool is_valid_addr_len(const MemState &state, Address addr, uint32_t len);
bool handle_access_violation(MemState &state, uint8_t *addr, bool write) noexcept;
Block alloc_block(MemState &mem, uint32_t size, const char *name, Address start_addr = user_main_memory_start);
Address alloc_at(MemState &state, Address address, uint32_t size, const char *name);
Address try_alloc_at(MemState &state, Address address, uint32_t size, const char *name);
void free(MemState &state, Address address);
uint32_t mem_available(MemState &state);
const char *mem_name(Address address, MemState &state);

struct MemAllocationToken {
    Address base = 0;
    uint32_t size = 0;
    uint64_t generation = 0;
    explicit operator bool() const { return generation != 0; }
    bool operator==(const MemAllocationToken &) const = default;
};

// Capture the identity of the one live allocation containing the entire range.
MemAllocationToken capture_mem_allocation(MemState &state, Address address, uint64_t length);
bool matches_mem_allocation(MemState &state, const MemAllocationToken &expected, Address address, uint64_t length);

// Conservative ownership: any CPU access to a shared host page cancels the
// pending readback. This never waits for the renderer from a fault callback.
struct GpuReadbackWatch {
    MemAllocationToken allocation;
    Address destination;
    uint32_t row_bytes, height, stride;
    std::atomic<bool> invalidated{ false };
};
bool gpu_readback_supported();
std::shared_ptr<GpuReadbackWatch> begin_gpu_readback_watch(MemState &state,
    const MemAllocationToken &allocation, Address destination,
    uint32_t row_bytes, uint32_t height, uint32_t stride);
void finish_gpu_readback_watch(MemState &state, const std::shared_ptr<GpuReadbackWatch> &watch);

// Commit completed GPU rows without firing CPU-write invalidation callbacks.
// Preserves all existing protected segments and row padding; contiguous guest view only.
// macOS uses a short-lived shared alias, never opening guest page permissions.
// Production callers must also pass the watch armed at the render boundary.
bool write_gpu_readback_rows(MemState &state, Address destination, const uint8_t *source,
    uint32_t row_bytes, uint32_t height, uint32_t destination_stride,
    const MemAllocationToken *expected = nullptr, const GpuReadbackWatch *watch = nullptr);
