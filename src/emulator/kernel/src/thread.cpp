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

#include <kernel/thread_functions.h>
#include <kernel/thread_state.h>
#include <psp2/kernel/error.h>
#include <psp2/types.h>

#include <kernel/state.h>

#include <cpu/functions.h>
#include <util/find.h>
#include <util/lock_and_find.h>
#include <util/resource.h>

#include <SDL_thread.h>

#include <cassert>
#include <cstring>

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
    const bool succeeded = run_thread(*thread, false);
    assert(succeeded);
    const uint32_t r0 = read_reg(*thread->cpu, 0);
    return r0;
}

SceUID create_thread(Ptr<const void> entry_point, KernelState &kernel, MemState &mem, const char *name, int init_priority, int stack_size, CallImport call_import, bool log_code) {
    WaitingThreadState waiting;
    waiting.name = name;

    SceUID thid = kernel.next_uid++;

    const ThreadStack::Deleter stack_deleter = [&mem](Address stack) {
        free(mem, stack);
    };

    const ThreadStatePtr thread = std::make_shared<ThreadState>();
    std::copy(name, name + KERNELOBJECT_MAX_NAME_LENGTH, thread->name);
    // TODO: needs testing
    if (init_priority & (SCE_KERNEL_DEFAULT_PRIORITY & 0xF0000000)) {
        thread->priority = init_priority - SCE_KERNEL_DEFAULT_PRIORITY + SCE_KERNEL_DEFAULT_PRIORITY_USER_INTERNAL;
    } else {
        thread->priority = init_priority;
    }
    thread->stack_size = stack_size;
    thread->stack = std::make_shared<ThreadStack>(alloc(mem, stack_size, "Stack"), stack_deleter);
    const Address stack_top = thread->stack->get() + stack_size;
    memset(Ptr<void>(thread->stack->get()).get(mem), 0xcc, stack_size);

    const CallSVC call_svc = [call_import, thid, &mem](uint32_t imm, Address pc) {
        assert(imm == 0);
        const uint32_t nid = *Ptr<uint32_t>(pc + 4).get(mem);
        call_import(nid, thid);
    };

    thread->cpu = init_cpu(entry_point.address(), stack_top, log_code, call_svc, mem);
    if (!thread->cpu) {
        return SCE_KERNEL_ERROR_ERROR;
    }

    const std::unique_lock<std::mutex> lock(kernel.mutex);
    kernel.threads.emplace(thid, thread);
    kernel.waiting_threads.emplace(thid, waiting);

    return thid;
}

int start_thread(KernelState &kernel, const SceUID &thid, SceSize arglen, const Ptr<void> &argp) {
    const std::unique_lock<std::mutex> lock(kernel.mutex);

    const WaitingThreadStates::const_iterator waiting = kernel.waiting_threads.find(thid);
    if (waiting == kernel.waiting_threads.end()) {
        return SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID;
    }

    const ThreadStatePtr thread = find(thid, kernel.threads);
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
            lock.unlock();
            res = run(*thread.cpu, callback);
            if (res == 1) {
                return true;
            }
            if (res < 0) {
                return false;
            }
            if (callback) {
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

bool run_on_current(ThreadState &thread, const Ptr<const void> entry_point, SceSize arglen, Ptr<void> &argp) {
    std::unique_lock<std::mutex> lock(thread.mutex);
    write_reg(*thread.cpu, 0, arglen);
    write_reg(*thread.cpu, 1, argp.address());
    write_pc(*thread.cpu, entry_point.address());
    lock.unlock();
    return run_thread(thread, true);
}
