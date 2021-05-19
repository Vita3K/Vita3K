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

#include <kernel/state.h>
#include <kernel/thread/thread_functions.h>
#include <kernel/thread/thread_state.h>

#include <cpu/functions.h>
#include <mem/ptr.h>
#include <util/align.h>
#include <util/arm.h>
#include <util/find.h>
#include <util/log.h>

#include <spdlog/fmt/fmt.h>

int CorenumAllocator::new_corenum() {
    const std::lock_guard<std::mutex> guard(lock);

    int size = 1;
    return alloc.allocate_from(0, size);
}

void CorenumAllocator::free_corenum(const int num) {
    const std::lock_guard<std::mutex> guard(lock);
    alloc.free(num, 1);
}

void CorenumAllocator::set_max_core_count(const std::size_t max) {
    const std::lock_guard<std::mutex> guard(lock);
    alloc.set_maximum(max);
}

bool KernelState::init(MemState &mem, CallImportFunc call_import, CPUBackend cpu_backend, bool cpu_opt) {
    constexpr std::size_t MAX_CORE_COUNT = 150;

    corenum_allocator.set_max_core_count(MAX_CORE_COUNT);
    exclusive_monitor = new_exclusive_monitor(MAX_CORE_COUNT);
    start_tick = { rtc_base_ticks() };
    base_tick = { rtc_base_ticks() };
    cpu_protocol = std::make_unique<CPUProtocol>(*this, mem, call_import);
    debugger.init(this);
    this->cpu_backend = cpu_backend;
    this->cpu_opt = cpu_opt;
    guest_func_runner = create_thread(*this, mem, "guest function runner");

    return true;
}

void KernelState::set_memory_watch(bool enabled) {
    std::lock_guard<std::mutex> lock(mutex);
    for (const auto &thread : threads) {
        auto &cpu = *thread.second->cpu;
        if (enabled != get_log_mem(cpu)) {
            if (enabled)
                set_log_mem(cpu, true);
            else
                set_log_mem(cpu, false);
        }
    }
}

void KernelState::invalidate_jit_cache(Address start, size_t length) {
    std::lock_guard<std::mutex> lock(mutex);
    for (auto thread : threads) {
        ::invalidate_jit_cache(*thread.second->cpu, start, length);
    }
}

ThreadStatePtr KernelState::get_thread(SceUID thread_id) {
    std::lock_guard<std::mutex> lock(mutex);
    auto it = threads.find(thread_id);
    if (it == threads.end()) {
        return nullptr;
    }
    return it->second;
}

Ptr<Ptr<void>> KernelState::get_thread_tls_addr(MemState &mem, SceUID thread_id, int key) {
    Ptr<Ptr<void>> address(0);
    //magic numbers taken from decompiled source. There is 0x400 unused bytes of unknown usage
    if (key <= 0x100 && key >= 0) {
        const ThreadStatePtr thread = util::find(thread_id, threads);
        address = thread->tls.get_ptr<Ptr<void>>() + key;
    } else {
        LOG_ERROR("Wrong tls slot index. TID:{} index:{}", thread_id, key);
    }
    return address;
}

void KernelState::stop_all_threads() {
    const std::lock_guard<std::mutex> lock(mutex);
    for (ThreadStatePtrs::iterator thread = threads.begin(); thread != threads.end(); ++thread) {
        exit_and_delete_thread(*thread->second);
    }
}

int KernelState::run_guest_function(Address callback_address, const std::vector<uint32_t> &args) {
    return ::run_guest_function(*this, *this->guest_func_runner, callback_address, args);
}
