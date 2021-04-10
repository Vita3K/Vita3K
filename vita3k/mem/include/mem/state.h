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

#include <mem/util.h>

#include <map>
#include <mutex>

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
