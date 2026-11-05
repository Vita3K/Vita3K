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
#include <cpu/common.h>
#include <mem/state.h>
#include <mem/util.h>
#include <util/types.h>

#include <atomic>
#include <condition_variable>
#include <map>
#include <mutex>
#include <optional>
#include <vector>

struct KernelState;

// A condition from the debugger, as a GDB agent expression
using BreakpointCondition = std::vector<uint8_t>;

struct Breakpoint {
    bool thumb_mode;
    unsigned char data[4];
    // The breakpoint stops a thread when one is true, or always when there are none
    std::vector<BreakpointCondition> conditions;
};

enum class WatchType : uint8_t {
    WRITE = 2, // matches GDB Z2
    READ = 3, // matches GDB Z3
    ACCESS = 4, // matches GDB Z4
};

struct WatchMemory {
    Address start;
    size_t size;
    WatchType type;
};

using Breakpoints = std::map<Address, Breakpoint>;

struct Debugger {
    Debugger() = delete;
    explicit Debugger(KernelState &kernel);

    bool watch_import_calls = false;
    bool watch_code = false;
    bool watch_memory = false;

    bool log_imports = false;
    bool log_exports = false;
    bool dump_elfs = false;

    // A watchpoint of each type and size can be at the same address
    void add_watch_memory_addr(Address addr, size_t size, WatchType type = WatchType::ACCESS);
    void remove_watch_memory_addr(Address addr, size_t size, WatchType type);
    void remove_all_watchpoints();
    // Adding a breakpoint where there is one replaces its conditions
    void add_breakpoint(MemState &mem, uint32_t addr, bool thumb_mode, std::vector<BreakpointCondition> conditions = {});
    void remove_breakpoint(MemState &mem, uint32_t addr);
    void remove_all_breakpoints(MemState &mem);
    // Checks whether a thread's access hits a watchpoint, and keeps the hit for the stop reply
    bool check_watchpoint(SceUID thread_id, Address addr, size_t size, bool is_write);
    // The watchpoint a thread hit since the last call or clear_watch_hits(), if any
    std::optional<WatchMemory> take_watch_hit(SceUID thread_id);
    void clear_watch_hits();
    // Whether the CPUs check memory accesses, for watch_memory or a watchpoint
    bool memory_watched();
    // Makes the CPUs check memory accesses as memory_watched() says
    void update_watches();
    void deinit();

    // Whether the breakpoint a thread hit stops it, which runs its conditions
    bool breakpoint_stops(MemState &mem, CPUState &cpu);
    // Runs the instruction under the breakpoint at the PC, for a thread the breakpoint doesn't stop.
    // The breakpoint stays for the other threads.
    GuestExit step_over_breakpoint(CPUState &cpu);

    // GDB breakpoint notification: signaled from the thread loop when a
    // thread hits a breakpoint and transitions to suspend.
    void notify_breakpoint(SceUID thread_id);

    std::mutex break_mutex;
    std::condition_variable break_cv;
    SceUID break_thread_id = 0;
    std::atomic<bool> has_break{ false };

private:
    std::mutex mutex;
    KernelState &parent;
    Breakpoints breakpoints;
    // Guest threads check the watchpoints on every access while one is set
    std::mutex watch_mutex;
    std::vector<WatchMemory> watchpoints;
    std::map<SceUID, WatchMemory> watch_hits;
};
