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
#include <kernel/thread_state.h>

#include <cpu/functions.h>
#include <mem/mem.h>
#include <mem/ptr.h>

Ptr<Ptr<void>> get_thread_tls_addr(KernelState &kernel, MemState &mem, SceUID thread_id, int key) {
    SlotToAddress &slot_to_address = kernel.tls[thread_id];

    const SlotToAddress::const_iterator existing = slot_to_address.find(key);
    if (existing != slot_to_address.end()) {
        return existing->second;
    }

    // TODO Use a finer-grained allocator.
    // TODO This is a memory leak.
    const Ptr<Ptr<void>> address(alloc<Ptr<void>>(mem, "TLS"));
    slot_to_address.insert(SlotToAddress::value_type(key, address));

    return address;
}

void stop_all_threads(KernelState &kernel) {
    const std::unique_lock<std::mutex> lock(kernel.mutex);
    for (ThreadStatePtrs::iterator thread = kernel.threads.begin(); thread != kernel.threads.end(); ++thread) {
        {
            const std::unique_lock<std::mutex> lock2(thread->second->mutex);
            thread->second->to_do = ThreadToDo::exit;
        }
        thread->second->something_to_do.notify_all();
        stop(*thread->second->cpu);
    }
}
