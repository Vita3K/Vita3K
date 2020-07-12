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

#include <kernel/functions.h>

#include <cpu/functions.h>
#include <util/find.h>
#include <util/lock_and_find.h>
#include <util/resource.h>

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
    write_reg(*thread->cpu, 0, params.arglen);
    write_reg(*thread->cpu, 1, params.argp.address());
    if (params.kernel->wait_for_debugger) {
        thread->to_do = ThreadToDo::wait;
        // Any following threads opened with thread_function will not wait.
        params.kernel->wait_for_debugger = false;
    }
    const bool succeeded = run_thread(*thread, false);
    assert(succeeded);
    const uint32_t r0 = read_reg(*thread->cpu, 0);

    const std::lock_guard<std::mutex> lock(thread->mutex);

    for (auto &waiting_thread : thread->waiting_threads) {
        waiting_thread->to_do = ThreadToDo::run;
        waiting_thread->something_to_do.notify_all();
    }

    thread->waiting_threads.clear();

    return r0;
}

SceUID create_thread(Ptr<const void> entry_point, KernelState &kernel, MemState &mem, const char *name, int init_priority, int stack_size, CPUDepInject &inject, const SceKernelThreadOptParam *option = nullptr) {
    SceUID thid = kernel.get_next_uid();

    const ThreadStack::Deleter stack_deleter = [&mem](Address stack) {
        free(mem, stack);
    };

    const CallSVC call_svc = [inject, thid, &mem](CPUState &cpu, uint32_t imm, Address pc) {
        assert(imm == 0);
        const uint32_t nid = *Ptr<uint32_t>(pc + 4).get(mem);
        inject.call_import(cpu, nid, thid);
    };
    inject.call_svc = call_svc;

    const ThreadStatePtr thread = std::make_shared<ThreadState>();
    thread->name = name;
    thread->entry_point = entry_point.address();
    // TODO: needs testing
    if (init_priority & (SCE_KERNEL_DEFAULT_PRIORITY & 0xF0000000)) {
        thread->priority = init_priority - SCE_KERNEL_DEFAULT_PRIORITY + SCE_KERNEL_DEFAULT_PRIORITY_USER_INTERNAL;
    } else {
        thread->priority = init_priority;
    }
    thread->stack_size = stack_size;
    auto alloc_name = fmt::format("Stack for thread {} (#{})", name, thid);
    thread->stack = std::make_shared<ThreadStack>(alloc(mem, stack_size, alloc_name.c_str()), stack_deleter);
    const Address stack_top = thread->stack->get() + stack_size;
    memset(Ptr<void>(thread->stack->get()).get(mem), 0xcc, stack_size);

    thread->cpu = init_cpu(thid, entry_point.address(), stack_top, mem, inject);
    if (!thread->cpu) {
        return SCE_KERNEL_ERROR_ERROR;
    }
    if (kernel.watch_code) {
        log_code_add(*thread->cpu);
    }
    if (kernel.watch_memory) {
        log_mem_add(*thread->cpu);
    }

    if (option) {
        write_reg(*thread->cpu, 0, option->attr);
        write_reg(*thread->cpu, 1, option->size);
    }

    thread->cpu_context = std::make_unique<CPUContext>();
    if (!thread->cpu_context) {
        return SCE_KERNEL_ERROR_ERROR;
    }

    alloc_name = fmt::format("TLS for thread {} (#{})", name, thid);

    auto tls_address = alloc(mem, 0x800, alloc_name.c_str()) + 0x800;

    write_tpidruro(*thread->cpu, tls_address);

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

int start_thread(KernelState &kernel, const SceUID &thid, SceSize arglen, const Ptr<void> &argp) {
    const std::unique_lock<std::mutex> lock(kernel.mutex);

    const KernelWaitingThreadStates::const_iterator waiting = kernel.waiting_threads.find(thid);
    if (waiting == kernel.waiting_threads.end()) {
        return SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID;
    }

    const ThreadStatePtr thread = util::find(thid, kernel.threads);
    assert(thread);

    ThreadParams params;
    params.kernel = &kernel;
    params.thid = thid;
    params.arglen = arglen;
    params.argp = argp;

    const std::function<void(SDL_Thread *)> delete_thread = [thread](SDL_Thread *running_thread) {
        {
            const std::unique_lock<std::mutex> lock(thread->mutex);
            thread->to_do = ThreadToDo::exit;
        }
        thread->something_to_do.notify_all(); // TODO Should this be notify_one()?

        raise_waiting_threads(thread.get());

        SDL_WaitThread(running_thread, nullptr);
    };

    const ThreadPtr running_thread(SDL_CreateThread(&thread_function, waiting->second.name.c_str(), &params), delete_thread);
    if (!running_thread) {
        return SCE_KERNEL_ERROR_THREAD_ERROR;
    }

    kernel.waiting_threads.erase(waiting);
    kernel.running_threads.emplace(thid, running_thread);
    SDL_SemWait(params.host_may_destroy_params.get());
    return SCE_KERNEL_OK;
}

Ptr<void> copy_stack(SceUID thid, SceUID thread_id, const Ptr<void> &argp, KernelState &kernel, MemState &mem) {
    const ThreadStatePtr new_thread = lock_and_find(thid, kernel.threads, kernel.mutex);
    const ThreadStatePtr old_thread = lock_and_find(thread_id, kernel.threads, kernel.mutex);

    const std::unique_lock<std::mutex> lock(kernel.mutex);

    const Address old_stack_address = old_thread->stack->get();
    const Address new_stack_address = new_thread->stack->get();

    const Address old_sp = read_sp(*old_thread->cpu);
    const Address old_stack_size = old_stack_address + old_thread->stack_size - old_sp;
    const Address new_sp = new_stack_address + new_thread->stack_size - old_stack_size;

    memcpy(Ptr<void>(new_sp).get(mem), Ptr<void>(old_sp).get(mem), old_stack_size);
    write_sp(*new_thread->cpu, new_sp);

    Ptr<void> new_argp = argp;
    if (old_stack_address < argp.address() && (old_stack_address + old_thread->stack_size > argp.address())) {
        const Address offset = old_stack_address + old_thread->stack_size - argp.address();
        new_argp = Ptr<void>(new_stack_address + new_thread->stack_size - offset);
    }
    return new_argp;
}

bool run_thread(ThreadState &thread, bool callback) {
    int res = 0;
    std::unique_lock<std::mutex> lock(thread.mutex);
    while (true) {
        switch (thread.to_do) {
        case ThreadToDo::exit:
            return true;
        case ThreadToDo::run:
        case ThreadToDo::step:
            lock.unlock();
            if (thread.to_do == ThreadToDo::step) {
                res = step(*thread.cpu, callback, thread.entry_point);
                thread.to_do = ThreadToDo::wait;
            } else
                res = run(*thread.cpu, callback, thread.entry_point);
#ifdef USE_GDBSTUB
            if (hit_breakpoint(*thread.cpu)) {
                thread.to_do = ThreadToDo::wait;
                LOG_INFO("Stopping thread \"{}\" at breakpoint.", thread.name);
            }
#endif
            if (res < 0) {
                LOG_ERROR("Thread {} experienced a unicorn error.", thread.name);
                return false;
            }
            if (callback) {
                return true;
            }
            if (res == 1) {
                const std::unique_lock<std::mutex> lock(thread.mutex);
                raise_waiting_threads(&thread);
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

bool run_callback(ThreadState &thread, Address &pc, Address &data) {
    std::unique_lock<std::mutex> lock(thread.mutex);
    write_reg(*thread.cpu, 0, data);
    write_pc(*thread.cpu, pc);
    lock.unlock();
    return run_thread(thread, true);
}

uint32_t run_on_current(ThreadState &thread, const Ptr<const void> entry_point, const std::list<uint32_t> &args) {
    std::vector<uint32_t> args_vector{ std::begin(args), std::end(args) };
    std::unique_lock<std::mutex> lock(thread.mutex);
    stop(*thread.cpu);
    lock.unlock();
    CPUContext ctx;
    save_context(*thread.cpu, ctx);
    assert(args_vector.size() < 4); // TODO support this by using stack
    for (int i = 0; i < args_vector.size(); ++i) {
        write_reg(*thread.cpu, i, args_vector[i]);
    }
    write_pc(*thread.cpu, entry_point.address());
    run(*thread.cpu, true, entry_point.address());

    load_context(*thread.cpu, ctx);
    auto out = read_reg(*thread.cpu, 0);

    run_thread(thread, false);
    return out;
}
