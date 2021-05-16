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

#include <kernel/thread/thread_functions.h>
#include <kernel/thread/thread_state.h>

#include <kernel/state.h>
#include <util/align.h>

#include <kernel/functions.h>

#include <cpu/functions.h>
#include <cpu/state.h>
#include <util/find.h>
#include <util/lock_and_find.h>

#include <SDL_thread.h>
#include <spdlog/fmt/fmt.h>
#include <util/log.h>

#include <cassert>
#include <cstring>
#include <memory>

struct ThreadParams {
    KernelState *kernel = nullptr;
    SceUID thid = SCE_KERNEL_ERROR_ILLEGAL_THREAD_ID;
    SceSize arglen = 0;
    Ptr<void> argp;
    std::shared_ptr<SDL_semaphore> host_may_destroy_params = std::shared_ptr<SDL_semaphore>(SDL_CreateSemaphore(0), SDL_DestroySemaphore);
};

static int SDLCALL thread_function(void *data) {
    assert(data != nullptr);
    const ThreadParams params = *static_cast<const ThreadParams *>(data);
    SDL_SemPost(params.host_may_destroy_params.get());
    const ThreadStatePtr thread = lock_and_find(params.thid, params.kernel->threads, params.kernel->mutex);
    if (params.kernel->wait_for_debugger) {
        thread->to_do = ThreadToDo::wait;
        // Any following threads opened with thread_function will not wait.
        params.kernel->wait_for_debugger = false;
    }
    const bool succeeded = run_on_current(*thread, Ptr<void>(thread->entry_point), params.arglen, params.argp);
    const uint32_t r0 = read_reg(*thread->cpu, 0);
    thread->returned_value = r0;

    std::lock_guard<std::mutex> lock(params.kernel->mutex);
    if (thread->to_do == ThreadToDo::exit || thread->to_do == ThreadToDo::exit_delete) {
        raise_waiting_threads(thread.get());
        params.kernel->waiting_threads.erase(thread->id);
    }
    if (thread->to_do == ThreadToDo::exit_delete) {
        params.kernel->corenum_allocator.free_corenum(thread->core_num);
        params.kernel->threads.erase(thread->id);
    }

    params.kernel->running_threads.erase(thread->id);

    return r0;
}

SceUID create_thread(Ptr<const void> entry_point, KernelState &kernel, MemState &mem, const char *name, int init_priority, int stack_size, const SceKernelThreadOptParam *option = nullptr) {
    SceUID thid = kernel.get_next_uid();

    const ThreadStatePtr thread = std::make_shared<ThreadState>();
    thread->name = name;
    thread->entry_point = entry_point.address();
    thread->id = thid;

    thread->core_num = kernel.corenum_allocator.new_corenum();
    if (thread->core_num < 0) {
        LOG_ERROR("Out of core number to allocate, use 0");
        thread->core_num = 0;
    }

    if (init_priority > SCE_KERNEL_LOWEST_PRIORITY_USER) {
        assert(SCE_KERNEL_HIGHEST_DEFAULT_PRIORITY <= init_priority && init_priority <= SCE_KERNEL_LOWEST_DEFAULT_PRIORITY);
        thread->priority = init_priority - SCE_KERNEL_DEFAULT_PRIORITY + SCE_KERNEL_GAME_DEFAULT_PRIORITY_ACTUAL;
    } else {
        thread->priority = init_priority;
    }
    thread->stack_size = stack_size;
    auto alloc_name = fmt::format("Stack for thread {} (#{})", name, thid);
    thread->stack = alloc_block(mem, stack_size, alloc_name.c_str());
    const Address stack_top = thread->stack.get() + stack_size;
    memset(thread->stack.get_ptr<void>().get(mem), 0xcc, stack_size);

    thread->cpu = init_cpu(kernel.cpu_backend, thid, static_cast<std::size_t>(thread->core_num), entry_point.address(), stack_top, mem, kernel.cpu_protocol);
    if (!thread->cpu) {
        return SCE_KERNEL_ERROR_ERROR;
    }
    if (kernel.watch_code) {
        set_log_code(*thread->cpu, true);
    }
    if (kernel.watch_memory) {
        set_log_mem(*thread->cpu, true);
    }

    if (option) {
        write_reg(*thread->cpu, 0, option->attr);
        write_reg(*thread->cpu, 1, option->size);
    }

    alloc_name = fmt::format("TLS for thread {} (#{})", name, thid);

    auto tls_size = kernel_tls_size + kernel.tls_msize;
    thread->tls = alloc_block(mem, tls_size, alloc_name.c_str());
    auto base_tls_address_ptr = thread->tls.get_ptr<uint8_t>();
    memset(base_tls_address_ptr.get(mem), 0, tls_size);

    if (kernel.tls_address) {
        auto user_tls_address_ptr = base_tls_address_ptr + kernel_tls_size;
        write_tpidruro(*thread->cpu, user_tls_address_ptr.address());
        assert(kernel.tls_psize <= kernel.tls_msize);
        memcpy(user_tls_address_ptr.get(mem), kernel.tls_address.get(mem), kernel.tls_psize);
    } else {
        write_tpidruro(*thread->cpu, 0);
    }
    WaitingThreadState waiting{ name };

    const std::unique_lock<std::mutex> lock(kernel.mutex);
    kernel.threads.emplace(thid, thread);
    kernel.waiting_threads.emplace(thid, waiting);

    return thid;
}

void raise_waiting_threads(ThreadState *thread) {
    for (auto t : thread->waiting_threads) {
        const std::unique_lock<std::mutex> lock(t->mutex);
        assert(t->to_do == ThreadToDo::wait);
        t->to_do = ThreadToDo::run;
        t->something_to_do.notify_one();
    }
    thread->waiting_threads.clear();
}

bool is_running(KernelState &kernel, ThreadState &thread) {
    return kernel.running_threads.find(thread.id) != kernel.running_threads.end();
}

int start_thread(KernelState &kernel, const SceUID &thid, SceSize arglen, const Ptr<void> &argp) {
    const std::unique_lock<std::mutex> lock(kernel.mutex);

    const KernelWaitingThreadStates::const_iterator waiting = kernel.waiting_threads.find(thid);
    if (waiting == kernel.waiting_threads.end()) {
        if (kernel.running_threads.find(thid) != kernel.running_threads.end()) {
            return SCE_KERNEL_ERROR_RUNNING;
        }
        return SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID;
    }

    const ThreadStatePtr thread = util::find(thid, kernel.threads);
    assert(thread);

    ThreadParams params;
    params.kernel = &kernel;
    params.thid = thid;
    params.arglen = arglen;
    params.argp = argp;

    const ThreadPtr running_thread(SDL_CreateThread(&thread_function, waiting->second.name.c_str(), &params), [thread](SDL_Thread *running_thread) {});
    if (!running_thread) {
        return SCE_KERNEL_ERROR_THREAD_ERROR;
    }

    kernel.waiting_threads.erase(waiting);
    kernel.running_threads.emplace(thid, running_thread);
    SDL_SemWait(params.host_may_destroy_params.get());
    return SCE_KERNEL_OK;
}

Ptr<void> copy_block_to_stack(ThreadState &thread, MemState &mem, const Ptr<void> &data, const int size) {
    const Address stack_top = thread.stack.get() + thread.stack_size;
    const Address sp = read_sp(*thread.cpu);
    assert(sp <= stack_top && sp >= thread.stack.get());
    assert(sp - thread.stack.get() >= size);
    const int aligned_size = align(size, 8);
    const Address data_addr = sp - aligned_size;
    memcpy(Ptr<uint8_t>(data_addr).get(mem), data.get(mem), size);

    write_sp(*thread.cpu, data_addr);

    return Ptr<void>(data_addr);
}

static bool run_thread(ThreadState &thread) {
    int res = 0;
    std::unique_lock<std::mutex> lock(thread.mutex);
    while (true) {
        switch (thread.to_do) {
        case ThreadToDo::exit_delete:
        case ThreadToDo::exit:
            return true;
        case ThreadToDo::run:
        case ThreadToDo::step:
            lock.unlock();
            if (thread.to_do == ThreadToDo::step) {
                res = step(*thread.cpu);
                thread.to_do = ThreadToDo::wait;

            } else
                res = run(*thread.cpu);

            if (thread.to_do == ThreadToDo::exit || thread.to_do == ThreadToDo::exit_delete)
                return true;

            if (res < 0) {
                LOG_ERROR("Thread {} experienced a unicorn error.", thread.name);
                return true;
            }

            if (hit_breakpoint(*thread.cpu)) {
                thread.to_do = ThreadToDo::wait;
            }

            if (res == 1) {
                thread.to_do = ThreadToDo::exit;
                return true;
            }
            lock.lock();

            break;
        case ThreadToDo::wait:
            thread.something_to_do.wait(lock);
            break;
        }
    }
}

int run_callback(KernelState &kernel, ThreadState &thread, const SceUID &thid, Address callback_address, const std::vector<uint32_t> &args) {
    bool should_wait = false;
    if (kernel.cpu_pool.available() == 0) {
        LOG_TRACE("CPU pool for callbacks is empty. Waiting.");
        should_wait = true;
    }
    auto cpu_item = kernel.cpu_pool.borrow();
    if (should_wait)
        LOG_TRACE("Got a cpu from CPU pool after a wait");
    auto cpu = cpu_item.get();

    CPUContext ctx = save_context(*thread.cpu);
    load_context(*cpu, ctx);
    write_tpidruro(*cpu, read_tpidruro(*thread.cpu));

    assert(args.size() <= 4);
    for (int i = 0; i < args.size(); i++) {
        write_reg(*cpu, i, args[i]);
    }
    // TODO: remaining arguments should be pushed into stack
    set_thread_id(*cpu, thid);
    write_pc(*cpu, callback_address);
    write_lr(*cpu, cpu->halt_instruction_pc);
    CPUStatePtr cpu_ptr = CPUStatePtr(cpu, [](CPUState *state) -> void {});
    thread.cpu.swap(cpu_ptr);
    auto res = run(*cpu);
    thread.cpu.swap(cpu_ptr);

    if (res < 0) {
        LOG_ERROR("Thread {} experienced a unicorn error.", thread.name);
        return -1;
    }

    return read_reg(*cpu, 0);
}

uint32_t run_on_current(ThreadState &thread, const Ptr<const void> entry_point, SceSize arglen, const Ptr<void> &argp) {
    CPUContext ctx = save_context(*thread.cpu);
    auto tpidruro = read_tpidruro(*thread.cpu);

    write_reg(*thread.cpu, 0, arglen);
    write_reg(*thread.cpu, 1, argp.address());
    write_pc(*thread.cpu, entry_point.address());
    write_lr(*thread.cpu, thread.cpu->halt_instruction_pc);
    set_thread_id(*thread.cpu, thread.id);

    run_thread(thread);
    auto out = read_reg(*thread.cpu, 0);

    load_context(*thread.cpu, ctx);
    write_tpidruro(*thread.cpu, tpidruro);
    return out;
}

void exit_thread(ThreadState &thread) {
    const auto last_to_do = thread.to_do;
    thread.to_do = ThreadToDo::exit;
    if (last_to_do == ThreadToDo::wait) {
        std::lock_guard<std::mutex> lock(thread.mutex);
        thread.something_to_do.notify_one();
    } else if (last_to_do == ThreadToDo::run) {
        stop(*thread.cpu);
    }
}

void exit_and_delete_thread(ThreadState &thread) {
    const auto last_to_do = thread.to_do;
    thread.to_do = ThreadToDo::exit_delete;
    if (last_to_do == ThreadToDo::wait) {
        std::lock_guard<std::mutex> lock(thread.mutex);
        thread.something_to_do.notify_one();
    } else if (last_to_do == ThreadToDo::run) {
        stop(*thread.cpu);
    }
}

void delete_thread(KernelState &kernel, ThreadState &thread) {
    kernel.corenum_allocator.free_corenum(thread.core_num);

    kernel.waiting_threads.erase(thread.id);
    raise_waiting_threads(&thread);
    kernel.threads.erase(thread.id);
}

int wait_thread_end(ThreadStatePtr &waiter, ThreadStatePtr &target, int *stat) {
    const std::lock_guard<std::mutex> waiter_lock(waiter->mutex);

    {
        const std::lock_guard<std::mutex> thread_lock(target->mutex);

        if (target->to_do == ThreadToDo::exit) {
            if (stat != nullptr) {
                *stat = target->returned_value;
            }
            return 0;
        }

        target->waiting_threads.push_back(waiter);
    }
    waiter->to_do = ThreadToDo::wait;
    stop(*waiter->cpu);
    return 0;
}

void ThreadSignal::wait() {
    std::unique_lock<std::mutex> lock(mutex);
    recv_cond.wait(lock, [&]() { return signaled; });
    signaled = false;
}

bool ThreadSignal::send() {
    std::unique_lock<std::mutex> lock(mutex);
    if (signaled) {
        return false;
    }
    signaled = true;
    recv_cond.notify_one();
    return true;
}
