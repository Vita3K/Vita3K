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

#include <kernel/debugger.h>
#include <kernel/state.h>

constexpr unsigned char THUMB_BREAKPOINT[2] = { 0x00, 0xBE };
constexpr unsigned char ARM_BREAKPOINT[4] = { 0x70, 0x00, 0x20, 0xE1 };

void Debugger::add_breakpoint(MemState &mem, uint32_t addr, bool thumb_mode) {
    const auto lock = std::lock_guard(mutex);
    Breakpoint bk;
    bk.thumb_mode = thumb_mode;
    if (thumb_mode) {
        std::memcpy(&bk.data, Ptr<uint8_t>(addr).get(mem), sizeof(THUMB_BREAKPOINT));
        std::memcpy(Ptr<uint8_t>(addr).get(mem), THUMB_BREAKPOINT, sizeof(THUMB_BREAKPOINT));
    } else {
        std::memcpy(&bk.data, Ptr<uint8_t>(addr).get(mem), sizeof(ARM_BREAKPOINT));
        std::memcpy(Ptr<uint8_t>(addr).get(mem), ARM_BREAKPOINT, sizeof(ARM_BREAKPOINT));
    }
    breakpoints.emplace(addr, bk);
    parent.invalidate_jit_cache(addr, 4);
}

void Debugger::remove_breakpoint(MemState &mem, uint32_t addr) {
    const auto lock = std::lock_guard(mutex);
    if (breakpoints.contains(addr)) {
        auto last = breakpoints[addr];
        std::memcpy(Ptr<uint8_t>(addr).get(mem), &last.data, last.thumb_mode ? sizeof(THUMB_BREAKPOINT) : sizeof(ARM_BREAKPOINT));
        breakpoints.erase(addr);
        parent.invalidate_jit_cache(addr, 4);
    }
}

Debugger::Debugger(KernelState &kernel)
    : parent(kernel) {
}

void Debugger::add_watch_memory_addr(Address addr, size_t size) {
    std::lock_guard<std::mutex> lock(mutex);
    watch_memory_addrs.emplace(addr, WatchMemory{ addr, size });
}

void Debugger::remove_watch_memory_addr(KernelState &state, Address addr) {
    std::lock_guard<std::mutex> lock(mutex);
    watch_memory_addrs.erase(addr);
}

// TODO use boost icl or interval tree instead if this turns out to be a significant bottleneck
Address Debugger::get_watch_memory_addr(Address addr) {
    std::lock_guard<std::mutex> lock(mutex);
    for (const auto &item : watch_memory_addrs) {
        if (item.second.start <= addr && addr < item.second.start + item.second.size) {
            return item.second.start;
        }
    }
    return 0;
}

void Debugger::update_watches() {
    parent.set_memory_watch(watch_memory);
}

void Debugger::deinit() {
    std::lock_guard<std::mutex> lock(mutex);
    breakpoints.clear();
    watch_memory_addrs.clear();
}
