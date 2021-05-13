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

#include <kernel/functions.h>
#include <kernel/state.h>
#include <kernel/thread/thread_functions.h>
#include <kernel/thread/thread_state.h>

#include <cpu/functions.h>
#include <mem/ptr.h>
#include <util/find.h>
#include <util/log.h>

#include <spdlog/fmt/fmt.h>

bool init(KernelState &kernel, MemState &mem, int cpu_pool_size, CPUProtocolBase *cpu_protocol, CPUBackend cpu_backend) {
    for (int i = 0; i < cpu_pool_size; ++i) {
        auto item = init_cpu(cpu_backend, 0, 0, 0, mem, cpu_protocol);
        kernel.cpu_pool.add(std::move(item));
    }

    kernel.exclusive_monitor = new_exclusive_monitor(100);
    kernel.start_tick = { rtc_base_ticks() };
    kernel.base_tick = { rtc_base_ticks() };
    kernel.cpu_protocol = cpu_protocol;
    return true;
}

Ptr<Ptr<void>> get_thread_tls_addr(KernelState &kernel, MemState &mem, SceUID thread_id, int key) {
    Ptr<Ptr<void>> address(0);
    //magic numbers taken from decompiled source. There is 0x400 unused bytes of unknown usage
    if (key <= 0x100 && key >= 0) {
        const ThreadStatePtr thread = util::find(thread_id, kernel.threads);
        address = thread->tls.get_ptr<Ptr<void>>() + key;
    } else {
        LOG_ERROR("Wrong tls slot index. TID:{} index:{}", thread_id, key);
    }
    return address;
}

void stop_all_threads(KernelState &kernel) {
    const std::lock_guard<std::mutex> lock(kernel.mutex);
    for (ThreadStatePtrs::iterator thread = kernel.threads.begin(); thread != kernel.threads.end(); ++thread) {
        exit_thread(*thread->second);
    }
}

void add_watch_memory_addr(KernelState &state, Address addr, size_t size) {
    state.watch_memory_addrs.emplace(addr, WatchMemory{ addr, size });
}

void remove_watch_memory_addr(KernelState &state, Address addr) {
    state.watch_memory_addrs.erase(addr);
}

// TODO use boost icl or interval tree instead if this turns out to be a significant bottleneck
Address get_watch_memory_addr(KernelState &state, Address addr) {
    for (const auto &item : state.watch_memory_addrs) {
        if (item.second.start <= addr && addr < item.second.start + item.second.size) {
            return item.second.start;
        }
    }
    return 0;
}

void update_watches(KernelState &state) {
    for (const auto &thread : state.threads) {
        auto &cpu = *thread.second->cpu;
        if (state.watch_code != get_log_code(cpu)) {
            if (state.watch_code)
                set_log_code(cpu, true);
            else
                set_log_code(cpu, false);
        }
        if (state.watch_memory != get_log_mem(cpu)) {
            if (state.watch_memory)
                set_log_mem(cpu, true);
            else
                set_log_mem(cpu, false);
        }
    }
}
