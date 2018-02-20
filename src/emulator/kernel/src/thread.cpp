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

#include <cpu/functions.h>
#include <util/resource.h>
#include <util/types.h>

#include <cassert>
#include <cstring>

ThreadStatePtr init_thread(Ptr<const void> entry_point, size_t stack_size, bool log_code, MemState &mem, CallImport call_import) {
    const ThreadStack::Deleter stack_deleter = [&mem](Address stack) {
        free(mem, stack);
    };

    const ThreadStatePtr thread = std::make_shared<ThreadState>();
    thread->stack = std::make_shared<ThreadStack>(alloc(mem, stack_size, "Stack"), stack_deleter);
    const Address stack_top = thread->stack->get() + stack_size;
    memset(Ptr<void>(thread->stack->get()).get(mem), 0xcc, stack_size);

    const CallSVC call_svc = [call_import, &mem](u32 imm, Address pc) {
        assert(imm == 0);
        const u32 nid = *Ptr<u32>(pc + 4).get(mem);
        call_import(nid);
    };

    thread->cpu = init_cpu(entry_point.address(), stack_top, log_code, call_svc, mem);
    if (!thread->cpu) {
        return ThreadStatePtr();
    }

    return thread;
}

bool run_thread(ThreadState &thread) {
    std::unique_lock<std::mutex> lock(thread.mutex);
    while (true) {
        switch (thread.to_do) {
        case ThreadToDo::exit:
            return true;
        case ThreadToDo::run:
            lock.unlock();
            if (!run(*thread.cpu)) {
                return false;
            }
            lock.lock();
            break;
        case ThreadToDo::wait:
            thread.something_to_do.wait(lock);
            break;
        }
    }
}
