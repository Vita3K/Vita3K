// Vita3K emulator project
// Copyright (C) 2025 Vita3K team
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

#ifdef TRACY_ENABLE
#include <tracy/Tracy.hpp>
#endif

#include <kernel/state.h>

#include <kernel/thread/thread_state.h>

#include <cpu/functions.h>
#include <mem/ptr.h>
#include <util/lock_and_find.h>
#include <util/log.h>

#include <SDL3/SDL_mutex.h>

int CorenumAllocator::new_corenum() {
    const std::lock_guard<std::mutex> guard(lock);

    uint32_t size = 1;
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

// TODO implement cross platform debug thread name setter and eliminate SDL thread
struct ThreadParams {
    KernelState *kernel = nullptr;
    SceUID thid = SCE_KERNEL_ERROR_ILLEGAL_THREAD_ID;
    SDL_Semaphore *host_may_destroy_params = nullptr;
};

static int SDLCALL thread_function(void *data) {
    assert(data != nullptr);
    const ThreadParams params = *static_cast<const ThreadParams *>(data);
    SDL_SignalSemaphore(params.host_may_destroy_params);
    const ThreadStatePtr thread = params.kernel->get_thread(params.thid);
#ifdef TRACY_ENABLE
    if (!thread->name.empty()) {
        tracy::SetThreadName(thread->name.c_str());
    } else {
        std::string th_name = "TID:" + std::to_string(thread->id);
        tracy::SetThreadName(th_name.c_str());
    }
#endif

    thread->run_loop();
    const uint32_t r0 = read_reg(*thread->cpu, 0);

    std::lock_guard<std::mutex> lock(params.kernel->mutex);
    params.kernel->threads.erase(thread->id);
    params.kernel->corenum_allocator.free_corenum(get_processor_id(*thread->cpu));

    return r0;
}

KernelState::KernelState()
    : debugger(*this) {
}

bool KernelState::init(MemState &mem, const CallImportFunc &call_import, bool cpu_opt) {
    constexpr std::size_t MAX_CORE_COUNT = 150;

    corenum_allocator.set_max_core_count(MAX_CORE_COUNT);
    exclusive_monitor = new_exclusive_monitor(MAX_CORE_COUNT);
    start_tick = rtc_get_ticks(rtc_base_ticks());
    base_tick = { rtc_base_ticks() };
    cpu_protocol = std::make_unique<CPUProtocol>(*this, mem, call_import);
    this->cpu_opt = cpu_opt;

    return true;
}

void KernelState::load_process_param(MemState &mem, Ptr<uint32_t> ptr) {
    const SceProcessParam *param = ptr.cast<SceProcessParam>().get(mem);
    if (param->version == 0) {
        // Homebrews built with old vitasdk
        process_param = nullptr;
        return;
    }
    process_param = ptr.cast<SceProcessParam>();
    // VAR_NID(__sce_libcparam, 0xDF084DFA)
    // no memory leak because we don't allocate memory for this variable intially
    export_nids[0xDF084DFA] = process_param.get(mem)->sce_libc_param.address();
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
    for (const auto &[_, thread] : threads) {
        ::invalidate_jit_cache(*thread->cpu, start, length);
    }
}

ThreadStatePtr KernelState::get_thread(SceUID thread_id) {
    return lock_and_find(thread_id, threads, mutex);
}

ThreadStatePtr KernelState::create_thread(MemState &mem, const char *name, Ptr<const void> entry_point) {
    return create_thread(mem, name, entry_point, SCE_KERNEL_DEFAULT_PRIORITY, SCE_KERNEL_THREAD_CPU_AFFINITY_MASK_DEFAULT, SCE_KERNEL_STACK_SIZE_USER_MAIN, nullptr);
}

ThreadStatePtr KernelState::create_thread(MemState &mem, const char *name, Ptr<const void> entry_point, int init_priority, SceInt32 affinity_mask, int stack_size, const SceKernelThreadOptParam *option) {
    ThreadStatePtr thread = std::make_shared<ThreadState>(get_next_uid(), *this, mem);
    if (thread->init(name, entry_point, init_priority, affinity_mask, stack_size, option) < 0)
        return nullptr;
    const auto lock = std::lock_guard(mutex);
    threads.emplace(thread->id, thread);

    ThreadParams params;
    params.kernel = this;
    params.thid = thread->id;

    params.host_may_destroy_params = SDL_CreateSemaphore(0);
    SDL_CreateThread(&thread_function, thread->name.c_str(), &params);
    SDL_WaitSemaphore(params.host_may_destroy_params);
    SDL_DestroySemaphore(params.host_may_destroy_params);
    return thread;
}

Ptr<Ptr<void>> KernelState::get_thread_tls_addr(MemState &mem, SceUID thread_id, int key) {
    Ptr<Ptr<void>> address(0);
    // magic numbers taken from decompiled source. There is 0x400 unused bytes of unknown usage
    if (key <= 0x100 && key >= 0) {
        const ThreadStatePtr thread = get_thread(thread_id);
        address = thread->tls.get_ptr<Ptr<void>>() + key;
    } else {
        LOG_ERROR("Wrong tls slot index. TID:{} index:{}", thread_id, key);
    }
    return address;
}

void KernelState::exit_delete_all_threads() {
    const std::lock_guard<std::mutex> lock(mutex);
    for (auto &[_, thread] : threads) {
        thread->exit_delete();
    }
}

void KernelState::pause_threads() {
    const std::lock_guard<std::mutex> lock(mutex);
    for (auto &[_, thread] : threads) {
        paused_threads_status[thread->id] = thread->status;
        if (thread->status == ThreadStatus::run)
            thread->suspend();
    }
}

void KernelState::resume_threads() {
    const std::lock_guard<std::mutex> lock(mutex);
    for (auto &[_, thread] : threads) {
        if (paused_threads_status[thread->id] == ThreadStatus::run)
            thread->resume();
    }
    paused_threads_status.clear();
}

SceKernelModuleInfo *KernelState::find_module_by_addr(Address address) {
    const auto lock = std::lock_guard(mutex);
    for (auto &[_, mod] : loaded_modules) {
        for (auto &seg : mod->info.segments) {
            if (!seg.size)
                continue;
            if (seg.vaddr.address() <= address && address <= seg.vaddr.address() + seg.memsz) {
                return &mod->info;
            }
        }
    }
    return nullptr;
}
