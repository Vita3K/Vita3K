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

#include <mem/allocator.h>
#include <mem/functions.h>
#include <mem/util.h>

#include <atomic>
#include <map>
#include <memory>
#include <mutex>
#include <vector>

struct AllocMemPage {
    uint32_t allocated : 4;
    uint32_t size : 28;
};

static_assert(sizeof(AllocMemPage) == 4);

typedef uint8_t *PagePtr;
typedef std::unique_ptr<uint8_t[], std::function<void(uint8_t *)>> Memory;
typedef std::unique_ptr<AllocMemPage[]> AllocPageTable;
typedef std::unique_ptr<PagePtr[]> PageTable;
typedef std::map<int, std::string> PageNameMap;

struct ProtectBlockInfo {
    uint32_t size = 0;
    ProtectCallback callback;
};

struct ProtectSegmentInfo {
    std::multimap<Address, ProtectBlockInfo> blocks;
    uint32_t size = 0;
    MemPerm perm = MemPerm::None;

    explicit ProtectSegmentInfo() = default;
    explicit ProtectSegmentInfo(uint32_t size, MemPerm perm)
        : size(size)
        , perm(perm) {
    }
};

typedef std::map<Address, ProtectSegmentInfo, std::greater<>> ProtectSegmentTrees;

struct MemExternalMapping {
    Address address;
    uint32_t size;
};

struct MemState {
    std::mutex generation_mutex;
    std::mutex protect_mutex;
    mutable std::mutex guest_mapping_mutex;
    std::atomic<bool> guest_mappings_active{ false };
    struct BackingPage {
        Address address;
        std::unique_ptr<uint8_t[]> retained;
        // Addresses mapped to this page by map_guest_memory; entries whose mapping later
        // changed are dropped lazily by guest_alias_ranges.
        std::vector<Address> aliases;
        explicit BackingPage(Address address)
            : address(address) {}
    };
    struct VirtualPage {
        std::shared_ptr<BackingPage> backing;
        MemPerm perm = MemPerm::None;
    };
    // Guest 4 KiB mappings, independent of host page size and GPU mappings.
    std::map<Address, VirtualPage> guest_mappings;
    // Flat view of guest_mappings for the JIT (dynarmic page_table, absolute offset) and Ptr<T>:
    // entry + addr is the host byte of a readable and writable page; null takes the checked path
    // (no access, read-only, decommitted, unallocated). Built before guest_mappings_active is set.
    std::unique_ptr<PagePtr[]> guest_page_table;

    uint32_t host_page_size = 0;
    Memory memory;
    AllocPageTable alloc_table;
    BitmapAllocator allocator;
    ProtectSegmentTrees protect_tree;

    PageNameMap page_name_map;

    bool use_page_table = false;
    PageTable page_table;
    std::map<uint64_t, MemExternalMapping, std::greater<>> external_mapping;
};
