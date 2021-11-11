// Vita3K emulator project
// Copyright (C) 2021 Vita3K team
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

#include <chrono>
#include <gxm/types.h>
#include <renderer/commands.h>
#include <renderer/state.h>
#include <renderer/types.h>

#include "driver_functions.h"
#include <renderer/gl/functions.h>

#include <renderer/functions.h>
#include <util/log.h>

namespace renderer {
COMMAND(handle_nop) {
    // Signal back to client
    int code_to_finish = helper.pop<int>();
    complete_command(renderer, helper, code_to_finish);
}

COMMAND(handle_signal_sync_object) {
    SceGxmSyncObject *sync = helper.pop<Ptr<SceGxmSyncObject>>().get(mem);
    renderer::subject_done(sync, renderer::SyncObjectSubject::Fragment);
}

// Client side function
void finish(State &state, Context &context) {
    // Wait for the code
    wait_for_status(state, &context.render_finish_status, state.last_scene_id, true);
}

int wait_for_status(State &state, int *status, int signal, bool wake_on_equal) {
    std::unique_lock<std::mutex> lock(state.command_finish_one_mutex);
    const bool wake_on_unequal = !wake_on_equal;
    if ((*status == signal) ^ wake_on_unequal) {
        // Signaled, return
        return *status;
    }

    // Wait for it to get signaled
    state.command_finish_one.wait(lock, [&]() { return (*status == signal) ^ wake_on_unequal; });
    return *status;
}

void wishlist(SceGxmSyncObject *sync_object, const SyncObjectSubject subjects) {
    {
        const std::lock_guard<std::mutex> mutex_guard(sync_object->lock);

        if (sync_object->done & subjects) {
            return;
        }
    }

    std::unique_lock<std::mutex> finish_mutex(sync_object->lock);
    sync_object->cond.wait(finish_mutex, [&]() { return sync_object->done & subjects; });
}

void subject_done(SceGxmSyncObject *sync_object, const SyncObjectSubject subjects) {
    {
        const std::lock_guard<std::mutex> mutex_guard(sync_object->lock);
        sync_object->done |= subjects;
    }

    sync_object->cond.notify_all();
}

void subject_in_progress(SceGxmSyncObject *sync_object, const SyncObjectSubject subjects) {
    const std::lock_guard<std::mutex> mutex_guard(sync_object->lock);
    sync_object->done &= ~subjects;
}

void submit_command_list(State &state, renderer::Context *context, CommandList &command_list) {
    command_list.context = context;
    state.command_buffer_queue.push(std::move(command_list));
}
} // namespace renderer
