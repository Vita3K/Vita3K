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

#include <mem/allocator.h>
#include <mem/functions.h>
#include <mem/util.h>

#include <map>
#include <memory>
#include <mutex>

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

    uint32_t page_size = 0;
    Memory memory;
    AllocPageTable alloc_table;
    BitmapAllocator allocator;
    ProtectSegmentTrees protect_tree;

    PageNameMap page_name_map;

    bool use_page_table = false;
    PageTable page_table;
    std::map<uint64_t, MemExternalMapping, std::greater<>> external_mapping;
};
