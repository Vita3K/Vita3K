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

#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

typedef uint32_t Address;
typedef size_t Generation;
typedef std::unique_ptr<uint8_t[], std::function<void(uint8_t *)>> Memory;
typedef std::vector<Generation> Allocated;
typedef std::map<Generation, std::string> GenerationNames;

struct CPUState;
struct MemState;

typedef void (*BreakpointCallback)(CPUState &, MemState &);

struct Breakpoint {
    bool gdb;
    bool thumb_mode;
    unsigned char data[4];
    BreakpointCallback callback;
};

struct MemState {
    size_t page_size = 0;
    Generation generation = 0;
    Memory memory;
    Allocated allocated_pages;
    std::mutex generation_mutex;
    GenerationNames generation_names;
    std::map<Address, Address> aligned_addr_to_original;
    std::map<Address, Breakpoint> breakpoints;
};

constexpr size_t KB(size_t kb) {
    return kb * 1024;
}

constexpr size_t MB(size_t mb) {
    return mb * KB(1024);
}

constexpr size_t GB(size_t gb) {
    return gb * MB(1024);
}

bool init(MemState &state);
Address alloc(MemState &state, size_t size, const char *name);
Address alloc(MemState &state, size_t size, const char *name, unsigned int alignment);
Address alloc_at(MemState &state, Address address, size_t size, const char *name);
void free(MemState &state, Address address);
uint32_t mem_available(MemState &state);
const char *mem_name(Address address, MemState &state);
void add_breakpoint(MemState &state, bool gdb, bool thumb_mode, uint32_t addr, BreakpointCallback callback);
void remove_breakpoint(MemState &state, uint32_t addr);
