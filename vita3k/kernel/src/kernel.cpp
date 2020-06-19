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
#include <kernel/thread/thread_state.h>

#include <cpu/functions.h>
#include <mem/mem.h>
#include <mem/ptr.h>

#include <spdlog/fmt/fmt.h>

#include <util/find.h>
#include <util/log.h>

Ptr<Ptr<void>> get_thread_tls_addr(KernelState &kernel, MemState &mem, SceUID thread_id, int key) {
    SlotToAddress &slot_to_address = kernel.tls[thread_id];

    const SlotToAddress::const_iterator existing = slot_to_address.find(key);
    if (existing != slot_to_address.end()) {
        return existing->second;
    }

    const ThreadStatePtr thread = util::find(thread_id, kernel.threads);

    Address tls = read_tpidruro(*thread->cpu) - 0x800 + key * 4;

    const Ptr<Ptr<void>> address(tls);

    if (!kernel.tls_address) {
        return address;
    } else {
        auto alloc_name = fmt::format("TLS for thread #{} {}", thread_id, key);
        // TODO Use a finer-grained allocator.
        // TODO This is a memory leak.
        Ptr<void> new_tls(alloc(mem, 4 * kernel.tls_msize, alloc_name.c_str()));
        memset(new_tls.get(mem), 0, 4 * kernel.tls_psize);
        memcpy(new_tls.get(mem), kernel.tls_address.get(mem), 4 * kernel.tls_psize);

        *address.get(mem) = new_tls;
        slot_to_address.insert(SlotToAddress::value_type(key, address));
        return address;
    }
}

void stop_all_threads(KernelState &kernel) {
    const std::lock_guard<std::mutex> lock(kernel.mutex);
    for (ThreadStatePtrs::iterator thread = kernel.threads.begin(); thread != kernel.threads.end(); ++thread) {
        {
            const std::lock_guard<std::mutex> lock2(thread->second->mutex);
            thread->second->to_do = ThreadToDo::exit;
        }
        thread->second->something_to_do.notify_all();
        stop(*thread->second->cpu);
    }
}

void add_watch_memory_addr(KernelState &state, Address addr, size_t size) {
    state.watch_memory_addrs.emplace(addr, WatchMemory{ addr, size });
}

void remove_watch_memory_addr(KernelState &state, Address addr) {
    state.watch_memory_addrs.erase(addr);
}

// TODO use boost icl or interval tree instead if this turns out to be a significant bottleneck
bool is_watch_memory_addr(KernelState &state, Address addr) {
    for (const auto &item : state.watch_memory_addrs) {
        if (item.second.start <= addr && addr < item.second.start + item.second.size) {
            return true;
        }
    }
    return false;
}

void update_watches(KernelState &state) {
    for (const auto &thread : state.threads) {
        auto &cpu = *thread.second->cpu;
        if (state.watch_code != log_code_exists(cpu)) {
            if (state.watch_code)
                log_code_add(cpu);
            else
                log_code_remove(cpu);
        }
        if (state.watch_memory != log_mem_exists(cpu)) {
            if (state.watch_memory)
                log_mem_add(cpu);
            else
                log_mem_remove(cpu);
        }
    }
}
