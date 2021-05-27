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

#include <cassert>
#include <cstring>
#include <memory>
#include <sstream>

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

int ThreadState::init(KernelState &kernel, const char *name, Ptr<const void> entry_point, int init_priority, int stack_size, const SceKernelThreadOptParam *option = nullptr) {
    constexpr size_t KERNEL_TLS_SIZE = 0x800;

    this->name = name;
    this->entry_point = entry_point.address();

    int core_num = kernel.corenum_allocator.new_corenum();
    if (core_num < 0) {
        LOG_ERROR("Out of core number to allocate, use 0");
        core_num = 0;
    }

    if (init_priority > SCE_KERNEL_LOWEST_PRIORITY_USER) {
        assert(SCE_KERNEL_HIGHEST_DEFAULT_PRIORITY <= init_priority && init_priority <= SCE_KERNEL_LOWEST_DEFAULT_PRIORITY);
        priority = init_priority - SCE_KERNEL_DEFAULT_PRIORITY + SCE_KERNEL_GAME_DEFAULT_PRIORITY_ACTUAL;
    } else {
        priority = init_priority;
    }
    this->stack_size = stack_size;
    start_tick = rtc_get_ticks(kernel.base_tick.tick);

    cpu = init_cpu(kernel.cpu_backend, kernel.cpu_opt, id, static_cast<std::size_t>(core_num), mem, kernel.cpu_protocol.get());
    if (!cpu) {
        return SCE_KERNEL_ERROR_ERROR;
    }
    if (kernel.debugger.watch_code) {
        set_log_code(*cpu, true);
    }
    if (kernel.debugger.watch_memory) {
        set_log_mem(*cpu, true);
    }

    std::string alloc_name = fmt::format("Stack for thread {} (#{})", name, id);
    stack = alloc_block(mem, stack_size, alloc_name.c_str());
    memset(stack.get_ptr<void>().get(mem), 0xcc, stack_size);

    alloc_name = fmt::format("TLS for thread {} (#{})", name, id);
    const size_t tls_size = KERNEL_TLS_SIZE + kernel.tls_msize;
    tls = alloc_block(mem, tls_size, alloc_name.c_str());
    const Ptr<uint8_t> base_tls_ptr = tls.get_ptr<uint8_t>();
    memset(base_tls_ptr.get(mem), 0, tls_size);

    if (kernel.tls_address) {
        const Ptr<uint8_t> user_tls_ptr = base_tls_ptr + KERNEL_TLS_SIZE;
        write_tpidruro(*cpu, user_tls_ptr.address());
        assert(kernel.tls_psize <= kernel.tls_msize);
        memcpy(user_tls_ptr.get(mem), kernel.tls_address.get(mem), kernel.tls_psize);
    } else {
        write_tpidruro(*cpu, 0);
    }

    CPUContext ctx;
    ctx.set_sp(stack_top());
    if (option) {
        ctx.cpu_registers[0] = option->attr;
        ctx.cpu_registers[1] = option->size;
    }
    this->init_cpu_ctx = ctx;

    return 0;
}

void ThreadState::flush_callback_requests() {
    if (!callback_requests.empty()) {
        const auto current = run_queue.begin();
        // Add resuming job
        ThreadJob job;
        job.ctx = save_context(*cpu);
        job.notify = current->notify;
        run_queue.erase(current);
        run_queue.push_front(job);

        // Add requested callback jobs
        for (auto it = callback_requests.rbegin(); it != callback_requests.rend(); ++it) {
            run_queue.push_front(*it);
        }
        callback_requests.clear();
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

    ThreadJob job;
    CPUContext ctx = init_cpu_ctx;
    job.args[0] = arglen;
    job.args[1] = argp.address();
    job.args_size = 2;
    ctx.set_pc(entry_point);
    ctx.set_lr(cpu->halt_instruction_pc);

    // Copy data to stack
    if (argp && arglen > 0) {
        const Address stack_top = stack.get() + stack_size;
        const int aligned_size = align(arglen, 8);
        const Address data_addr = stack_top - aligned_size;
        memcpy(Ptr<uint8_t>(data_addr).get(mem), argp.get(mem), arglen);
        job.args[1] = data_addr;
        ctx.set_sp(data_addr);
    }
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

bool ThreadState::run_loop() {
    int res = 0;
    RunQueue::iterator current_job;
    std::unique_lock<std::mutex> lock(mutex);
    while (true) {
        switch (to_do) {
        case ThreadToDo::exit:
            raise_waiting_threads();
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
                push_arguments(*current_job);
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
                continue;

            if (res < 0) {
                LOG_ERROR("Thread {} experienced a unicorn error.", name);
                if (current_job->notify) {
                    current_job->notify(0xDEADDEAD);
                }
                run_queue.erase(current_job);
                break;
            }

            if (hit_breakpoint(*cpu) || to_do == ThreadToDo::suspend) {
                ThreadJob job;
                job.ctx = save_context(*cpu);
                job.notify = current_job->notify;
                run_queue.erase(current_job);
                run_queue.push_front(job);
                update_status(ThreadStatus::suspend);
                to_do = ThreadToDo::wait;
            }

            if (res) {
                if (current_job->notify) {
                    current_job->notify(read_reg(*cpu, 0));
                }
                run_queue.erase(current_job);
            }
            break;
        case ThreadToDo::wait:
            something_to_do.wait(lock);
            break;
        }
    }
}

void ThreadState::push_arguments(ThreadJob &job) {
    Address sp = job.ctx.get_sp();
    for (size_t i = 0; i < std::min(job.args_size, static_cast<size_t>(4)); i++) {
        job.ctx.cpu_registers[i] = job.args[i];
    }
    if (job.args_size > 4) {
        // TODO align to 16 bytes
        const size_t remain_size = job.args_size - 4;
        sp -= 4 * remain_size;
        memcpy(Ptr<uint32_t>(sp).get(mem), &job.args[4], remain_size * 4);
    }
    job.ctx.set_sp(sp);
}

int ThreadState::run_guest_function(Address callback_address, const std::vector<uint32_t> &args) {
    std::unique_lock<std::mutex> thread_lock(mutex);
    ThreadJob job;
    CPUContext ctx = init_cpu_ctx;
    std::copy(args.begin(), args.end(), job.args.begin());
    job.args_size = args.size();
    ctx.set_pc(callback_address);
    ctx.set_lr(cpu->halt_instruction_pc);
    job.ctx = ctx;

    std::mutex notify_mutex;
    std::condition_variable notify_cond;
    bool notify_done = false;
    int notify_res = 0;

    job.notify = [&](int res) {
        std::lock_guard<std::mutex> lock(notify_mutex);
        notify_done = true;
        notify_res = res;
        notify_cond.notify_one();
    };

    // Push a job
    to_do = ThreadToDo::run;
    run_queue.push_back(job);
    something_to_do.notify_one();

    // Wait until job finishes
    thread_lock.unlock();
    std::unique_lock<std::mutex> lock(notify_mutex);
    notify_cond.wait(lock, [&]() { return notify_done; });

    return notify_res;
}

void ThreadState::stop_loop() {
    std::lock_guard<std::mutex> lock(mutex);
    const ThreadToDo last_to_do = to_do;
    to_do = ThreadToDo::exit;
    if (last_to_do == ThreadToDo::wait) {
        something_to_do.notify_one();
    } else {
        stop(*cpu);
    }
}

ThreadState::ThreadState(SceUID id, MemState &mem)
    : id(id)
    , mem(mem) {
}

void ThreadState::update_status(ThreadStatus status, std::optional<ThreadStatus> expected) {
    if (expected)
        assert(expected.value() == this->status);

    this->status = status;
    status_cond.notify_all();

    if (status == ThreadStatus::dormant) {
        raise_waiting_threads();
    }
}

Address ThreadState::stack_top() const {
    return stack.get() + stack_size;
}

void ThreadState::clear_run_queue() {
    const auto lock = std::lock_guard(mutex);
    if (!run_queue.empty()) {
        const auto top = run_queue.begin();
        if (top->notify) {
            top->notify(read_reg(*cpu, 0));
        }
        run_queue.clear();
    }
}

void ThreadState::request_callback(Address callback_address, const std::vector<uint32_t> &args, const std::function<void(int res)> notify) {
    const auto thread_lock = std::lock_guard(mutex);
    ThreadJob job;
    CPUContext ctx = save_context(*cpu);
    std::copy(args.begin(), args.end(), job.args.begin());
    job.args_size = args.size();
    ctx.set_pc(callback_address);
    ctx.set_lr(cpu->halt_instruction_pc);
    job.ctx = ctx;
    job.notify = notify;

    callback_requests.push_back(job);
}

void ThreadState::suspend() {
    assert(to_do == ThreadToDo::run);
    to_do = ThreadToDo::suspend;
    stop(*cpu);
}

void ThreadState::resume(bool step) {
    assert(to_do == ThreadToDo::wait);
    to_do = step ? ThreadToDo::step : ThreadToDo::run;
    something_to_do.notify_one();
}

std::string ThreadState::log_stack_traceback(KernelState &kernel) const {
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