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

#include "SceThreadmgrCoredumpTime.h"

#include <kernel/thread/thread_functions.h>
#include <util/lock_and_find.h>

EXPORT(int, sceKernelExitThread, int status) {
    const ThreadStatePtr thread = lock_and_find(thread_id, host.kernel.threads, host.kernel.mutex);
    const std::lock_guard<std::mutex> lock(thread->mutex);

    thread->to_do = ThreadToDo::exit;
    stop(*thread->cpu);
    thread->something_to_do.notify_all();

    raise_waiting_threads(thread.get());

    return status;
}

BRIDGE_IMPL(sceKernelExitThread)
