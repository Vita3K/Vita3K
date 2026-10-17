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

#include <cpu/state.h>
#include <kernel/state.h>
#include <kernel/sync_primitives.h>
#include <kernel/thread/thread_state.h>
#include <mem/functions.h>

#include <cstdint>
#include <span>
#include <string>

namespace threadmgr {

inline bool copy_sync_guest_bytes(MemState &mem, Address address, void *data, uint32_t size, bool write) {
    if (!size)
        return true;
    const GuestMemoryRange range{ address, size };
    const uint64_t pin = pin_guest_memory(mem, std::span(&range, 1));
    if (!pin)
        return false;
    auto *bytes = static_cast<uint8_t *>(data);
    for (uint32_t i = 0; i < size; ++i) {
        auto *guest = Ptr<uint8_t>(address + i).get(mem);
        if (write)
            *guest = bytes[i];
        else
            bytes[i] = *guest;
    }
    unpin_guest_memory(mem, pin);
    return true;
}

template <typename T>
inline std::shared_ptr<T> find_process_sync_object(KernelState &kernel, const ThreadState &caller, SceUID uid) {
    const auto object = kernel.objects.find<T>(uid);
    if (!object)
        return nullptr;
    MemState &mem = caller.cpu->mem;
    return (object->address_space ? object->address_space == &mem : kernel.is_primary_address_space(mem)) ? object : nullptr;
}

inline SemaphorePtr find_semaphore(KernelState &kernel, const ThreadState &caller, SceUID uid) {
    return find_process_sync_object<Semaphore>(kernel, caller, uid);
}

inline std::shared_ptr<HeavyMutex> find_heavy_mutex(KernelState &kernel, const ThreadState &caller, SceUID uid) {
    return find_process_sync_object<HeavyMutex>(kernel, caller, uid);
}

inline bool copy_lw_mutex_name(MemState &mem, Ptr<const char> name, std::string &out) {
    if (!name)
        return false;
    for (uint32_t i = 0; i < 4096; ++i) {
        const uint64_t address = uint64_t(name.address()) + i;
        if (address > UINT32_MAX)
            return false;
        const GuestMemoryRange range{ static_cast<Address>(address), 1 };
        const uint64_t pin = pin_guest_memory(mem, std::span(&range, 1));
        if (!pin)
            return false;
        const char ch = *Ptr<const char>(range.address).get(mem);
        unpin_guest_memory(mem, pin);
        if (!ch)
            return true;
        out.push_back(ch);
    }
    return false;
}

inline std::shared_ptr<LwMutex> find_lw_mutex(KernelState &kernel, const ThreadState &caller, Ptr<SceKernelLwMutexWork> workarea) {
    if (!workarea || !is_valid_addr_len(caller.cpu->mem, workarea.address(), sizeof(SceKernelLwMutexWork)))
        return nullptr;
    const auto mutex = kernel.objects.find<LwMutex>(workarea.get(caller.cpu->mem)->uid);
    return mutex && mutex->address_space == &caller.cpu->mem && mutex->workarea.address() == workarea.address() ? mutex : nullptr;
}

inline std::shared_ptr<LwCond> find_lw_cond(KernelState &kernel, const ThreadState &caller, Ptr<SceKernelLwCondWork> workarea) {
    if (!workarea || !is_valid_addr_len(caller.cpu->mem, workarea.address(), sizeof(SceKernelLwCondWork)))
        return nullptr;
    const auto condition = kernel.objects.find<LwCond>(workarea.get(caller.cpu->mem)->uid);
    return condition && condition->address_space == &caller.cpu->mem && condition->workarea.address() == workarea.address() ? condition : nullptr;
}

} // namespace threadmgr