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

#include <chrono>
#include <renderer/commands.h>
#include <renderer/driver_functions.h>
#include <renderer/state.h>
#include <renderer/types.h>

#include <display/state.h>
#include <renderer/gl/functions.h>
#include <renderer/vulkan/functions.h>

#include <renderer/functions.h>
#include <util/tracy.h>

namespace renderer {
COMMAND(handle_nop) {
    TRACY_FUNC_COMMANDS(handle_nop);
    // Signal back to client
    int code_to_finish = helper.pop<int>();
    complete_command(renderer, helper, code_to_finish);
}

COMMAND(handle_signal_sync_object) {
    TRACY_FUNC_COMMANDS(handle_signal_sync_object);
    SceGxmSyncObject *sync = helper.pop<Ptr<SceGxmSyncObject>>().get(mem);
    const uint32_t timestamp = helper.pop<uint32_t>();

    if (features.enable_memory_mapping && config.current_config.high_accuracy) {
        assert(renderer.current_backend == renderer::Backend::Vulkan);
        vulkan::signal_sync_object(dynamic_cast<vulkan::VKState &>(renderer), sync, timestamp);
    } else {
        renderer::subject_done(sync, timestamp);
    }
}

COMMAND(handle_wait_sync_object) {
    TRACY_FUNC_COMMANDS(handle_wait_sync_object);
    SceGxmSyncObject *sync = helper.pop<Ptr<SceGxmSyncObject>>().get(mem);
    const uint32_t timestamp = helper.pop<uint32_t>();

    renderer::wishlist(sync, timestamp);
}

COMMAND(handle_notification) {
    TRACY_FUNC_COMMANDS(handle_notification);
    SceGxmNotification notif = helper.pop<SceGxmNotification>();

    {
        std::unique_lock<std::mutex> lock(renderer.notification_mutex);
        uint32_t *val = notif.address.get(mem);
        if (val) // Ratchet and clank Trilogy request this
            *val = notif.value;
    }
    renderer.notification_ready.notify_all();
}

COMMAND(new_frame) {
    TRACY_FUNC_COMMANDS(new_frame);
    DisplayFrameInfo *next_frame = helper.pop<DisplayFrameInfo *>();

    if (next_frame) {
        // set the predicted frame as the next one to render
        DisplayState *display = helper.pop<DisplayState *>();
        std::lock_guard<std::mutex> guard(display->display_info_mutex);
        display->next_rendered_frame = *next_frame;
        delete next_frame;

        renderer.should_display = true;
    }

    if (renderer.current_backend == Backend::Vulkan) {
        vulkan::new_frame(*reinterpret_cast<vulkan::VKContext *>(renderer.context));
    }
}

// Client side function
void finish(State &state, Context *context) {
    // Add NOP then wait for it
    renderer::send_single_command(state, context, renderer::CommandOpcode::Nop, true, 1);
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

bool wishlist(SceGxmSyncObject *sync_object, const uint32_t timestamp, const int32_t timeout_micros) {
    std::unique_lock<std::mutex> lock(sync_object->lock);
    if (sync_object->timestamp_current < timestamp) {
        const auto &pred = [&]() { return sync_object->timestamp_current >= timestamp; };

        if (timeout_micros == -1) {
            sync_object->cond.wait(lock, pred);
            return true;
        } else {
            return sync_object->cond.wait_for(lock, std::chrono::microseconds(timeout_micros), pred);
        }
    } else {
        return true;
    }
}

void subject_done(SceGxmSyncObject *sync_object, const uint32_t timestamp) {
    assert(sync_object->timestamp_ahead >= timestamp);
    {
        std::unique_lock<std::mutex> lock(sync_object->lock);
        sync_object->timestamp_current = std::max(sync_object->timestamp_current.load(), timestamp);
    }
    // maybe notify_one is enough
    sync_object->cond.notify_all();
}

void submit_command_list(State &state, renderer::Context *context, CommandList &command_list) {
    command_list.context = context;
    state.command_buffer_queue.push(std::move(command_list));
}
} // namespace renderer
