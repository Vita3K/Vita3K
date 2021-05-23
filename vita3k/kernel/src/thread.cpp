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

#include <cpu/functions.h>
#include <kernel/thread/thread_state.h>

#include <kernel/state.h>
#include <mem/ptr.h>
#include <util/align.h>

#include <cpu/functions.h>
#include <util/find.h>
#include <util/lock_and_find.h>

#include <spdlog/fmt/fmt.h>
#include <util/log.h>

#include <SDL_thread.h>

#include <cassert>
#include <cstring>
#include <memory>
#include <sstream>

struct ThreadParams {
    KernelState *kernel = nullptr;
    SceUID thid = SCE_KERNEL_ERROR_ILLEGAL_THREAD_ID;
    std::shared_ptr<SDL_semaphore> host_may_destroy_params = std::shared_ptr<SDL_semaphore>(SDL_CreateSemaphore(0), SDL_DestroySemaphore);
};

static int SDLCALL thread_function(void *data) {
    assert(data != nullptr);
    const ThreadParams params = *static_cast<const ThreadParams *>(data);
    SDL_SemPost(params.host_may_destroy_params.get());
    const ThreadStatePtr thread = lock_and_find(params.thid, params.kernel->threads, params.kernel->mutex);
    thread->run_loop();
    const uint32_t r0 = read_reg(*thread->cpu, 0);
    thread->returned_value = r0;

    std::lock_guard<std::mutex> lock(params.kernel->mutex);
    thread->raise_waiting_threads();
    params.kernel->threads.erase(thread->id);
    params.kernel->corenum_allocator.free_corenum(get_processor_id(*thread->cpu));

    return r0;
}

SceUID ThreadState::create(Ptr<const void> entry_point, KernelState &kernel, MemState &mem, const char *name, int init_priority, int stack_size, const SceKernelThreadOptParam *option = nullptr) {
    SceUID thid = kernel.get_next_uid();

    const ThreadStatePtr thread = std::make_shared<ThreadState>();
    thread->name = name;
    thread->entry_point = entry_point.address();
    thread->id = thid;

    int core_num = kernel.corenum_allocator.new_corenum();
    if (core_num < 0) {
        LOG_ERROR("Out of core number to allocate, use 0");
        core_num = 0;
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

    thread->cpu = init_cpu(kernel.cpu_backend, kernel.cpu_opt, thid, static_cast<std::size_t>(core_num), entry_point.address(), stack_top, mem, kernel.cpu_protocol.get());
    if (!thread->cpu) {
        return SCE_KERNEL_ERROR_ERROR;
    }
    if (kernel.debugger.watch_code) {
        set_log_code(*thread->cpu, true);
    }
    if (kernel.debugger.watch_memory) {
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

void ThreadState::flush_callback_requests() {
    if (!jobs_to_add.empty()) {
        const auto current = run_queue.begin();
        // Add resuming job
        ThreadJob job;
        job.ctx = save_context(*cpu);
        job.notify = current->notify;
        run_queue.erase(current);
        run_queue.push_front(job);

        // Add requested callback jobs
        for (auto it = jobs_to_add.rbegin(); it != jobs_to_add.rend(); ++it) {
            run_queue.push_front(*it);
        }
        jobs_to_add.clear();
        stop(*cpu);
    }
}

void ThreadState::raise_waiting_threads() {
    for (auto t : waiting_threads) {
        const std::unique_lock<std::mutex> lock(t->mutex);
        assert(t->status == ThreadStatus::wait);
        t->status = ThreadStatus::run;
        t->status_cond.notify_all();
    }
    waiting_threads.clear();
}

int ThreadState::start(KernelState &kernel, SceSize arglen, const Ptr<void> &argp) {
    if (status == ThreadStatus::run)
        return SCE_KERNEL_ERROR_RUNNING;
    std::unique_lock<std::mutex> thread_lock(mutex);

    CPUContext ctx = save_context(*cpu);
    ctx.cpu_registers[0] = arglen;
    ctx.cpu_registers[1] = argp.address();
    ctx.set_pc(entry_point);
    ctx.set_lr(cpu->halt_instruction_pc);

    ThreadJob job;
    job.ctx = ctx;

    run_queue.push_front(job);

    if (kernel.debugger.wait_for_debugger) {
        to_do = ThreadToDo::suspend;
        status = ThreadStatus::suspend;
        kernel.debugger.wait_for_debugger = false;
    } else {
        to_do = ThreadToDo::run;
    }
    something_to_do.notify_one();

    return SCE_KERNEL_OK;
}

Ptr<void> ThreadState::copy_block_to_stack(MemState &mem, const Ptr<void> &data, const int size) {
    std::unique_lock<std::mutex> thread_lock(mutex);
    const Address stack_top = stack.get() + stack_size;
    const Address sp = read_sp(*cpu);
    assert(sp <= stack_top && sp >= stack.get());
    assert(sp - stack.get() >= size);
    const int aligned_size = align(size, 8);
    const Address data_addr = sp - aligned_size;
    memcpy(Ptr<uint8_t>(data_addr).get(mem), data.get(mem), size);

    write_sp(*cpu, data_addr);

    return Ptr<void>(data_addr);
}

bool ThreadState::run_loop() {
    int res = 0;
    RunQueue::iterator current_job;
    std::unique_lock<std::mutex> lock(mutex);
    while (true) {
        switch (to_do) {
        case ThreadToDo::exit:
            return true;
        case ThreadToDo::run:
        case ThreadToDo::step:
            // Pop a job to do
            if (run_queue.empty()) {
                update_status(ThreadStatus::dormant);
                to_do = ThreadToDo::wait;
                break;
            }

            current_job = run_queue.begin();
            if (!current_job->in_progress) {
                load_context(*cpu, current_job->ctx);
                current_job->in_progress = true;
            }
            update_status(ThreadStatus::run);

            // Run the cpu
            lock.unlock();
            if (to_do == ThreadToDo::step) {
                res = step(*cpu);
                to_do = ThreadToDo::suspend;

            } else
                res = run(*cpu);
            lock.lock();

            // Handle errors
            if (to_do == ThreadToDo::exit)
                return true;

            if (res < 0) {
                LOG_ERROR("Thread {} experienced a unicorn error.", name);
                if (current_job->notify) {
                    current_job->notify(0xDEADDEAD);
                }
                run_queue.erase(current_job);
                break;
            }

            if (hit_breakpoint(*cpu)) {
                to_do = ThreadToDo::suspend;
            }

            if (to_do == ThreadToDo::suspend) {
                ThreadJob job;
                job.ctx = save_context(*cpu);
                job.notify = current_job->notify;
                run_queue.erase(current_job);
                run_queue.push_front(job);
                update_status(ThreadStatus::suspend);
            }

            if (res == 1) {
                if (current_job->notify) {
                    current_job->notify(read_reg(*cpu, 0));
                }
                run_queue.erase(current_job);
            }
            break;
        case ThreadToDo::suspend:
        case ThreadToDo::wait:
            something_to_do.wait(lock);
            break;
        }
    }
}

int ThreadState::run_guest_function(Address callback_address, const std::vector<uint32_t> &args) {
    std::unique_lock<std::mutex> thread_lock(mutex);
    std::mutex notify_mutex;
    std::condition_variable notify_cond;
    bool done = false;
    int res = 0;

    ThreadJob job;

    job.notify = [&](int res2) {
        std::lock_guard<std::mutex> lock(notify_mutex);
        done = true;
        res = res2;
        notify_cond.notify_one();
    };

    CPUContext ctx = init_cpu_ctx;

    assert(args.size() <= 4);
    for (int i = 0; i < args.size(); i++) {
        ctx.cpu_registers[i] = args[i];
    }

    ctx.set_pc(callback_address);
    ctx.set_lr(cpu->halt_instruction_pc);
    job.ctx = ctx;
    // TODO: remaining arguments should be pushed into stack

    to_do = ThreadToDo::run;
    run_queue.push_back(job);
    something_to_do.notify_one();

    thread_lock.unlock();
    std::unique_lock<std::mutex> lock(notify_mutex);
    notify_cond.wait(lock, [&]() { return done; });

    return res;
}

void ThreadState::update_status(ThreadStatus status, std::optional<ThreadStatus> expected) {
    if (expected) {
        assert(expected.value() == this->status);
    }
    this->status = status;
    status_cond.notify_all();

    if (status == ThreadStatus::dormant) {
        raise_waiting_threads();
    }
}

void ThreadState::clear_run_queue() {
    if (!run_queue.empty()) {
        const auto top = run_queue.begin();
        if (top->notify) {
            top->notify(read_reg(*cpu, 0));
        }
        run_queue.clear();
    }
}

void ThreadState::request_callback(Address callback_address, const std::vector<uint32_t> &args, const std::function<void(int res)> notify) {
    std::unique_lock<std::mutex> thread_lock(mutex);
    ThreadJob job;
    CPUContext ctx = save_context(*cpu);
    job.notify = notify;
    assert(args.size() <= 4);
    for (int i = 0; i < args.size(); i++) {
        ctx.cpu_registers[i] = args[i];
    }

    ctx.set_pc(callback_address);
    ctx.set_lr(cpu->halt_instruction_pc);
    job.ctx = ctx;
    // TODO: remaining arguments should be pushed into stack

    jobs_to_add.push_back(job);
}

void ThreadState::halt() {
    std::lock_guard<std::mutex> lock(mutex);
    clear_run_queue();
    stop(*cpu);
}

void ThreadState::exit() {
    std::lock_guard<std::mutex> lock(mutex);
    const auto last_to_do = to_do;
    to_do = ThreadToDo::exit;
    clear_run_queue();
    if (last_to_do == ThreadToDo::wait) {
        something_to_do.notify_one();
    } else {
        stop(*cpu);
    }
}

void ThreadState::suspend() {
    assert(to_do == ThreadToDo::run);
    to_do = ThreadToDo::suspend;
    stop(*cpu);
}

void ThreadState::resume(bool step) {
    assert(to_do == ThreadToDo::suspend);
    to_do = step ? ThreadToDo::step : ThreadToDo::run;
    something_to_do.notify_one();
}

std::string ThreadState::log_stack_traceback(KernelState &kernel, MemState &mem) {
    constexpr Address START_OFFSET = 0;
    constexpr Address END_OFFSET = 1024;
    std::stringstream ss;
    const Address sp = read_sp(*cpu);
    for (Address addr = sp - START_OFFSET; addr <= sp + END_OFFSET; addr += 4) {
        const Address value = *Ptr<uint32_t>(addr).get(mem);
        const auto mod = kernel.find_module_by_addr(value);
        if (mod)
            ss << fmt::format("{} (module: {})\n", log_hex(value), mod->module_name);
    }
    return ss.str();
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
