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

#include <mem/allocator.h>
#include <mem/util.h>

#include <array>
#include <map>
#include <mutex>
#include <set>

struct MemPage {
    uint32_t allocated : 4;
    uint32_t size : 28;
};

static_assert(sizeof(MemPage) == 4);

typedef std::unique_ptr<uint8_t[], std::function<void(uint8_t *)>> Memory;
typedef std::unique_ptr<MemPage[], std::function<void(MemPage *)>> PageTable;
typedef std::map<int, std::string> PageNameMap;

struct WriteProtect {
    Address addr = 0;
    size_t size = 0;
    std::vector<WriteProtectCallback> callbacks;

    WriteProtect() = delete;
    explicit WriteProtect(Address addr)
        : addr(addr) {}
    explicit WriteProtect(Address addr, size_t size, WriteProtectCallback callback)
        : addr(addr)
        , size(size) {
        callbacks.push_back(callback);
        callbacks.reserve(5);
    }

    bool operator<(const WriteProtect &other) const {
        return this->addr < other.addr;
    }
};

typedef std::set<WriteProtect> WriteProtectTree;

struct MemState {
    std::mutex generation_mutex;
    std::mutex protect_mutex;

    size_t page_size = 0;
    Memory memory;
    PageTable page_table;
    BitmapAllocator allocator;
    WriteProtectTree write_protect_tree;

    PageNameMap page_name_map;
};
