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

#include <cpu/functions.h>
#include <kernel/thread/thread_state.h>

#include <kernel/state.h>
#include <mem/ptr.h>
#include <util/align.h>

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

int ThreadState::init(const char *name, Ptr<const void> entry_point, int init_priority, SceInt32 affinity_mask, int stack_size, const SceKernelThreadOptParam *option = nullptr) {
    constexpr size_t KERNEL_TLS_SIZE = 0x800;

    // the stack size should be page-aligned
    stack_size = align(stack_size, KiB(4));

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
    this->affinity_mask = affinity_mask;
    this->stack_size = stack_size;
    start_tick = rtc_get_ticks(kernel.base_tick.tick);
    last_vblank_waited = 0;

    cpu = init_cpu(kernel.cpu_opt, id, static_cast<std::size_t>(core_num), mem, kernel.cpu_protocol.get());
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

    int *tls_array = tls.get_ptr<int>().get(mem);

    tls_array[TLS_PROCESS_ID] = 1; // stubbed. unused
    tls_array[TLS_THREAD_ID] = id;
    tls_array[TLS_SP_TOP] = stack.get();
    tls_array[TLS_SP_BOTTOM] = stack.get() + stack_size;
    tls_array[TLS_CURRENT_PRIORITY] = priority;
    tls_array[TLS_CPU_AFFINITY_MASK] = affinity_mask;

    const Ptr<uint8_t> user_tls_ptr = base_tls_ptr + KERNEL_TLS_SIZE;
    write_tpidruro(*cpu, user_tls_ptr.address());
    if (kernel.tls_address) {
        assert(kernel.tls_psize <= kernel.tls_msize);
        memcpy(user_tls_ptr.get(mem), kernel.tls_address.get(mem), kernel.tls_psize);
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

void ThreadState::raise_waiting_threads() {
    for (const auto &t : waiting_threads) {
        const std::unique_lock<std::mutex> lock(t->mutex);
        assert(t->status == ThreadStatus::wait);
        t->status = ThreadStatus::run;
        t->status_cond.notify_all();
    }
    waiting_threads.clear();
}

int ThreadState::start(SceSize arglen, const Ptr<void> argp, bool run_entry_callback) {
    if (status == ThreadStatus::run || call_level > 0)
        return SCE_KERNEL_ERROR_RUNNING;
    std::unique_lock<std::mutex> thread_lock(mutex);

    run_start_callback = run_entry_callback;
    call_level = 1;
    load_context(*cpu, init_cpu_ctx);
    write_pc(*cpu, entry_point);
    write_lr(*cpu, cpu->halt_instruction_pc);
    write_reg(*cpu, 0, arglen);

    // Copy data to stack
    if (argp && arglen > 0) {
        const Address data_addr = stack_alloc(*cpu, align(arglen, 8));
        memcpy(Ptr<uint8_t>(data_addr).get(mem), argp.get(mem), arglen);
        write_reg(*cpu, 1, data_addr);
    } else {
        write_reg(*cpu, 1, 0);
    }

    if (kernel.debugger.wait_for_debugger) {
        to_do = ThreadToDo::suspend;
        status = ThreadStatus::suspend;
        kernel.debugger.wait_for_debugger = false;
    } else {
        to_do = ThreadToDo::run;
        status = ThreadStatus::run;
    }
    something_to_do.notify_one();

    return SCE_KERNEL_OK;
}

void ThreadState::exit(SceInt32 status) {
    std::lock_guard<std::mutex> guard(mutex);
    run_end_callback = true;
    call_level = 0;
    returned_value = static_cast<uint32_t>(status);
}

void ThreadState::exit_delete(bool exit) {
    std::lock_guard<std::mutex> lock(mutex);

    run_end_callback = exit;

    const ThreadToDo last_to_do = to_do;
    to_do = ThreadToDo::remove;
    if (last_to_do == ThreadToDo::wait) {
        something_to_do.notify_one();
    } else {
        stop(*cpu);
    }
}

bool ThreadState::run_loop() {
    int res = 0;
    int run_level = std::max(call_level, 1);

    std::unique_lock<std::mutex> lock(mutex);

    auto run_thread_end_callback = [&]() {
        if (!run_end_callback)
            return;
        run_end_callback = false;

        if (!kernel.thread_event_end)
            return;

        ThreadToDo old_to_do = to_do;
        int old_call_level = call_level;
        uint32_t old_returned_value = returned_value;
        to_do = ThreadToDo::run;
        call_level = 1;

        lock.unlock();
        int ret = run_callback(kernel.thread_event_end.address(), { SCE_KERNEL_THREAD_EVENT_TYPE_END, static_cast<uint32_t>(id), 0, kernel.thread_event_end_arg });
        if (ret != 0)
            LOG_WARN("Thread start event handler returned {}", log_hex(ret));
        lock.lock();

        to_do = old_to_do;
        call_level = old_call_level;
        returned_value = old_returned_value;
    };

    while (true) {
        switch (to_do) {
        case ThreadToDo::remove:
            if (run_level == 1) {
                run_thread_end_callback();
                update_status(ThreadStatus::dormant);
            }

            return true;
        case ThreadToDo::run:
        case ThreadToDo::step:

            if (call_level == 0) {
                run_thread_end_callback();

                // nothing to do
                update_status(ThreadStatus::dormant);
                to_do = ThreadToDo::wait;
                break;
            }

            update_status(ThreadStatus::run);

            lock.unlock();

            if (run_start_callback) {
                run_start_callback = false;

                if (kernel.thread_event_start) {
                    int ret = run_callback(kernel.thread_event_start.address(), { SCE_KERNEL_THREAD_EVENT_TYPE_START, static_cast<uint32_t>(id), 0, kernel.thread_event_start_arg });
                    if (ret != 0)
                        LOG_WARN("Thread start event handler returned {}", log_hex(ret));
                }
            }

            // Run the cpu
            if (to_do == ThreadToDo::step) {
                res = step(*cpu);
                to_do = ThreadToDo::suspend;

            } else
                res = run(*cpu);

            // handle svc call if this was what stopped the cpu
            if (cpu->svc_called) {
                cpu->protocol->call_svc(*cpu, cpu->svc_called, read_pc(*cpu), *this);
            }

            lock.lock();

            // Handle errors
            if (to_do == ThreadToDo::remove)
                continue;

            if (res < 0) {
                LOG_ERROR("Thread {} ({}) experienced a cpu error.", name, cpu->thread_id);
                returned_value = 0xDEADDEAD;
                call_level--;
                if (call_level > 0)
                    // only return if we are inside a callback
                    return true;
                break;
            }

            if (hit_breakpoint(*cpu) || to_do == ThreadToDo::suspend) {
                update_status(ThreadStatus::suspend);
                to_do = ThreadToDo::wait;
            }

            if (call_level < run_level && run_level > 1)
                // exit requested, exit this callback now
                return true;

            if (res) {
                returned_value = read_reg(*cpu, 0);
                call_level--;
                if (call_level > 0)
                    // only return if we are inside a callback
                    return true;
            }
            break;
        case ThreadToDo::wait:
            something_to_do.wait(lock);
            break;
        case ThreadToDo::suspend:
            update_status(ThreadStatus::suspend);
            something_to_do.wait(lock);
            break;
        }
    }
}

void ThreadState::push_arguments(const std::vector<uint32_t> &args) {
    Address sp = read_sp(*cpu);
    for (size_t i = 0; i < std::min(args.size(), static_cast<size_t>(4)); i++) {
        write_reg(*cpu, i, args[i]);
    }
    if (args.size() > 4) {
        // TODO align to 16 bytes
        const size_t remain_size = args.size() - 4;
        sp -= 4 * remain_size;
        memcpy(Ptr<uint32_t>(sp).get(mem), &args[4], remain_size * 4);
    }
    write_sp(*cpu, sp);
}

uint32_t ThreadState::run_callback(Address callback_address, const std::vector<uint32_t> &args) {
    if (call_level == 0) {
        LOG_ERROR("run_callback should not be called as the first thread entry");
        return 0;
    }

    // first save the current context
    const CPUContext previous_ctx = save_context(*cpu);
    const uint32_t previous_tpidruro = read_tpidruro(*cpu);

    std::unique_lock<std::mutex> thread_lock(mutex);
    call_level++;
    // we shouldn't have to clean the context I believe
    write_pc(*cpu, callback_address);
    write_lr(*cpu, cpu->halt_instruction_pc);
    push_arguments(args);
    thread_lock.unlock();

    // unlock but then immediately lock back in the run_loop function
    // shouldn't cause an issue, but maybe we could use a recursive mutex instead
    run_loop();

    thread_lock.lock();

    // restore the previous context
    // actually, in most case I don't think this is necessary as the caller
    // and the callee should respect the same calling convention
    // but do it just in case
    load_context(*cpu, previous_ctx);
    write_tpidruro(*cpu, previous_tpidruro);

    return returned_value;
}

uint32_t ThreadState::run_guest_function(Address callback_address, SceSize args, const Ptr<void> argp) {
    // save the previous entry point, just in case
    const auto old_entry_point = entry_point;
    entry_point = callback_address;

    start(args, argp);
    {
        // wait for the function to return
        std::unique_lock<std::mutex> lock(mutex);
        if (status != ThreadStatus::dormant || to_do == ThreadToDo::run) {
            status_cond.wait(lock, [&]() {
                return status == ThreadStatus::dormant && to_do != ThreadToDo::run;
            });
        }
    }

    entry_point = old_entry_point;
    return returned_value;
}

ThreadState::ThreadState(SceUID id, KernelState &kernel, MemState &mem)
    : id(id)
    , kernel(kernel)
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

void ThreadState::suspend() {
    assert(to_do == ThreadToDo::run);
    to_do = ThreadToDo::suspend;
    stop(*cpu);
}

void ThreadState::resume(bool step) {
    assert(to_do == ThreadToDo::wait || to_do == ThreadToDo::suspend);

    {
        const auto thread_lock = std::lock_guard(mutex);
        to_do = step ? ThreadToDo::step : ThreadToDo::run;
    }
    something_to_do.notify_one();
}

std::string ThreadState::log_stack_traceback() const {
    constexpr Address START_OFFSET = 0;
    constexpr Address END_OFFSET = 1024;
    std::string str;
    const Address sp = read_sp(*cpu);
    for (Address addr = sp - START_OFFSET; addr <= sp + END_OFFSET; addr += 4) {
        if (Ptr<uint32_t>(addr).valid(mem)) {
            const Address value = *Ptr<uint32_t>(addr).get(mem);
            const auto mod = kernel.find_module_by_addr(value);
            if (mod)
                fmt::format_to(std::back_inserter(str), "0x{:X} (module: {})\n", value, mod->module_name);
        }
    }
    return str;
}
