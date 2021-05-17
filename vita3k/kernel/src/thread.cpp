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
    std::shared_ptr<SDL_semaphore> host_may_destroy_params = std::shared_ptr<SDL_semaphore>(SDL_CreateSemaphore(0), SDL_DestroySemaphore);
};

static bool run_thread(ThreadState &thread);

static int SDLCALL thread_function(void *data) {
    assert(data != nullptr);
    const ThreadParams params = *static_cast<const ThreadParams *>(data);
    SDL_SemPost(params.host_may_destroy_params.get());
    const ThreadStatePtr thread = lock_and_find(params.thid, params.kernel->threads, params.kernel->mutex);
    run_thread(*thread);
    const uint32_t r0 = read_reg(*thread->cpu, 0);
    thread->returned_value = r0;

    std::lock_guard<std::mutex> lock(params.kernel->mutex);
    raise_waiting_threads(thread.get());
    params.kernel->threads.erase(thread->id);
    params.kernel->corenum_allocator.free_corenum(thread->core_num);

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

    thread->cpu = init_cpu(kernel.cpu_backend, kernel.cpu_opt, thid, static_cast<std::size_t>(thread->core_num), entry_point.address(), stack_top, mem, kernel.cpu_protocol.get());
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

    thread->init_cpu_ctx = save_context(*thread->cpu);

    const std::unique_lock<std::mutex> lock(kernel.mutex);
    kernel.threads.emplace(thid, thread);

    ThreadParams params;
    params.kernel = &kernel;
    params.thid = thid;

    SDL_CreateThread(&thread_function, thread->name.c_str(), &params);
    SDL_SemWait(params.host_may_destroy_params.get());
    return thid;
}

ThreadStatePtr create_thread(KernelState &kernel, MemState &mem, const char *name) {
    constexpr size_t DEFAULT_STACK_SIZE = 0x1000;
    const SceUID id = create_thread(Ptr<void>(0), kernel, mem, name, SCE_KERNEL_DEFAULT_PRIORITY, DEFAULT_STACK_SIZE, nullptr);
    return lock_and_find(id, kernel.threads, kernel.mutex);
}

void raise_waiting_threads(ThreadState *thread) {
    for (auto t : thread->waiting_threads) {
        const std::unique_lock<std::mutex> lock(t->mutex);
        assert(t->status == ThreadStatus::wait);
        t->status = ThreadStatus::run;
        t->status_cond.notify_all();
    }
    thread->waiting_threads.clear();
}

bool is_running(KernelState &kernel, ThreadState &thread) {
    return thread.status == ThreadStatus::run;
}

int start_thread(KernelState &kernel, const SceUID &thid, SceSize arglen, const Ptr<void> &argp) {
    const std::unique_lock<std::mutex> lock(kernel.mutex);

    const ThreadStatePtr thread = util::find(thid, kernel.threads);
    if (!thread) {
        return SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID;
    }
    if (thread->status == ThreadStatus::run)
        return SCE_KERNEL_ERROR_RUNNING;
    std::unique_lock<std::mutex> thread_lock(thread->mutex);

    CPUContext ctx = save_context(*thread->cpu);
    ctx.cpu_registers[0] = arglen;
    ctx.cpu_registers[1] = argp.address();
    ctx.set_pc(thread->entry_point);
    ctx.set_lr(thread->cpu->halt_instruction_pc);

    ThreadJob job;
    job.ctx = ctx;

    thread->run_queue.push_front(job);

    if (kernel.wait_for_debugger) {
        thread->to_do = ThreadToDo::wait;
        kernel.wait_for_debugger = false;
    } else {
        thread->to_do = ThreadToDo::run;
    }
    thread->something_to_do.notify_one();

    return SCE_KERNEL_OK;
}

Ptr<void> copy_block_to_stack(ThreadState &thread, MemState &mem, const Ptr<void> &data, const int size) {
    std::unique_lock<std::mutex> thread_lock(thread.mutex);
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
    RunQueue::iterator current_job;
    std::unique_lock<std::mutex> lock(thread.mutex);
    while (true) {
        switch (thread.to_do) {
        case ThreadToDo::exit:
            return true;
        case ThreadToDo::run:
        case ThreadToDo::step:
            // Pop a job to do
            if (thread.run_queue.empty()) {
                update_status(thread, ThreadStatus::dormant);
                thread.to_do = ThreadToDo::wait;
                break;
            }

            current_job = thread.run_queue.begin();
            if (!current_job->in_progress) {
                load_context(*thread.cpu, current_job->ctx);
                current_job->in_progress = true;
            }
            update_status(thread, ThreadStatus::run);

            // Run the cpu
            lock.unlock();
            if (thread.to_do == ThreadToDo::step) {
                res = step(*thread.cpu);
                thread.to_do = ThreadToDo::wait;

            } else
                res = run(*thread.cpu);
            lock.lock();

            // Handle errors
            if (thread.to_do == ThreadToDo::exit)
                return true;

            if (res < 0) {
                LOG_ERROR("Thread {} experienced a unicorn error.", thread.name);
                if (current_job->notify) {
                    current_job->notify(0xDEADDEAD);
                }
                thread.run_queue.erase(current_job);
                break;
            }

            if (hit_breakpoint(*thread.cpu)) {
                thread.to_do = ThreadToDo::wait;
            }

            if (res == 1) {
                if (current_job->notify) {
                    current_job->notify(read_reg(*thread.cpu, 0));
                }
                thread.run_queue.erase(current_job);
            }
            break;
        case ThreadToDo::wait:
            thread.something_to_do.wait(lock);
            break;
        }
    }
}

int run_guest_function(KernelState &kernel, ThreadState &thread, Address callback_address, const std::vector<uint32_t> &args) {
    std::unique_lock<std::mutex> thread_lock(thread.mutex);
    std::mutex mutex;
    std::condition_variable cond;
    bool done = false;
    int res = 0;

    ThreadJob job;

    job.notify = [&](int res2) {
        std::lock_guard<std::mutex> lock(mutex);
        done = true;
        res = res2;
        cond.notify_one();
    };

    CPUContext ctx = thread.init_cpu_ctx;

    assert(args.size() <= 4);
    for (int i = 0; i < args.size(); i++) {
        ctx.cpu_registers[i] = args[i];
    }

    ctx.set_pc(callback_address);
    ctx.set_lr(thread.cpu->halt_instruction_pc);
    job.ctx = ctx;
    // TODO: remaining arguments should be pushed into stack

    thread.to_do = ThreadToDo::run;
    thread.run_queue.push_back(job);
    thread.something_to_do.notify_one();

    thread_lock.unlock();
    std::unique_lock<std::mutex> lock(mutex);
    cond.wait(lock, [&]() { return done; });

    return res;
}

void update_status(ThreadState &thread, ThreadStatus status, std::optional<ThreadStatus> expected) {
    if (expected) {
        assert(expected.value() == thread.status);
    }
    thread.status = status;
    thread.status_cond.notify_all();

    if (status == ThreadStatus::dormant) {
        raise_waiting_threads(&thread);
    }
}

static void clear_run_queue(ThreadState &thread) {
    if (!thread.run_queue.empty()) {
        const auto top = thread.run_queue.begin();
        if (top->notify) {
            top->notify(read_reg(*thread.cpu, 0));
        }
        thread.run_queue.clear();
    }
}

void request_callback(ThreadState &thread, Address callback_address, const std::vector<uint32_t> &args, const std::function<void(int res)> notify) {
    std::unique_lock<std::mutex> thread_lock(thread.mutex);
    ThreadJob job;
    CPUContext ctx = save_context(*thread.cpu);
    job.notify = notify;
    assert(args.size() <= 4);
    for (int i = 0; i < args.size(); i++) {
        ctx.cpu_registers[i] = args[i];
    }

    ctx.set_pc(callback_address);
    ctx.set_lr(thread.cpu->halt_instruction_pc);
    job.ctx = ctx;
    // TODO: remaining arguments should be pushed into stack

    thread.jobs_to_add.push_back(job);
}

void exit_thread(ThreadState &thread) {
    std::lock_guard<std::mutex> lock(thread.mutex);
    clear_run_queue(thread);
    stop(*thread.cpu);
}

void exit_and_delete_thread(ThreadState &thread) {
    std::lock_guard<std::mutex> lock(thread.mutex);
    const auto last_to_do = thread.to_do;
    thread.to_do = ThreadToDo::exit;
    clear_run_queue(thread);
    if (last_to_do == ThreadToDo::wait) {
        thread.something_to_do.notify_one();
    } else {
        stop(*thread.cpu);
    }
}

void delete_thread(KernelState &kernel, ThreadState &thread) {
    std::lock_guard<std::mutex> lock(thread.mutex);
    assert(thread.to_do == ThreadToDo::wait);
    thread.to_do = ThreadToDo::exit;
    thread.something_to_do.notify_one();
}

int wait_thread_end(ThreadStatePtr &waiter, ThreadStatePtr &target, int *stat) {
    std::unique_lock<std::mutex> waiter_lock(waiter->mutex);
    {
        const std::unique_lock<std::mutex> thread_lock(target->mutex);
        if (target->status == ThreadStatus::dormant) {
            if (stat != nullptr) {
                *stat = target->returned_value;
            }
            return 0;
        }

        target->waiting_threads.push_back(waiter);
    }

    waiter->status = ThreadStatus::wait;
    waiter->status_cond.wait(waiter_lock, [&]() { return waiter->status == ThreadStatus::run; });
    return 0;
}

void wait_thread_status(ThreadState &thread, ThreadStatus status) {
    std::unique_lock<std::mutex> lock(thread.mutex);
    thread.status_cond.wait(lock, [&]() { return thread.status == status; });
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
