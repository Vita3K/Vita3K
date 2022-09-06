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

#include <renderer/commands.h>
#include <renderer/driver_functions.h>
#include <renderer/functions.h>
#include <renderer/state.h>
#include <renderer/types.h>

#include <renderer/vulkan/types.h>

#include <config/state.h>
#include <functional>
#include <util/log.h>

struct FeatureState;

namespace renderer {
Command *generic_command_allocate() {
    return new Command;
}

void generic_command_free(Command *cmd) {
    delete cmd;
}

void complete_command(State &state, CommandHelper &helper, const int code) {
    auto lock = std::unique_lock(state.command_finish_one_mutex);
    helper.complete(code);
    state.command_finish_one.notify_all();
}

bool is_cmd_ready(MemState &mem, CommandList &command_list) {
    // we check if the cmd starts with a WaitSyncObject and if this is the case if it is ready
    if (!command_list.first || command_list.first->opcode != CommandOpcode::WaitSyncObject)
        return true;

    SceGxmSyncObject *sync = reinterpret_cast<Ptr<SceGxmSyncObject> *>(&command_list.first->data[0])->get(mem);
    const uint32_t timestamp = *reinterpret_cast<uint32_t *>(&command_list.first->data[sizeof(uint32_t)]);

    return sync->timestamp_current >= timestamp;
}

static bool wait_cmd(MemState &mem, CommandList &command_list) {
    // we assume here that the cmd starts with a WaitSyncObject

    SceGxmSyncObject *sync = reinterpret_cast<Ptr<SceGxmSyncObject> *>(&command_list.first->data[0])->get(mem);
    const uint32_t timestamp = *reinterpret_cast<uint32_t *>(&command_list.first->data[sizeof(uint32_t)]);

    // wait 500 micro seconds and then return in case should_display is set to true
    return renderer::wishlist(sync, timestamp, 500);
}

static void process_batch(renderer::State &state, const FeatureState &features, MemState &mem, Config &config, CommandList &command_list) {
    using CommandHandlerFunc = decltype(cmd_handle_set_context);

    const static std::map<CommandOpcode, CommandHandlerFunc *> handlers = {
        { CommandOpcode::SetContext, cmd_handle_set_context },
        { CommandOpcode::SyncSurfaceData, cmd_handle_sync_surface_data },
        { CommandOpcode::MidSceneFlush, cmd_handle_mid_scene_flush },
        { CommandOpcode::CreateContext, cmd_handle_create_context },
        { CommandOpcode::CreateRenderTarget, cmd_handle_create_render_target },
        { CommandOpcode::MemoryMap, cmd_handle_memory_map },
        { CommandOpcode::MemoryUnmap, cmd_handle_memory_unmap },
        { CommandOpcode::Draw, cmd_handle_draw },
        { CommandOpcode::TransferCopy, cmd_handle_transfer_copy },
        { CommandOpcode::TransferDownscale, cmd_handle_transfer_downscale },
        { CommandOpcode::TransferFill, cmd_handle_transfer_fill },
        { CommandOpcode::Nop, cmd_handle_nop },
        { CommandOpcode::SetState, cmd_handle_set_state },
        { CommandOpcode::SignalSyncObject, cmd_handle_signal_sync_object },
        { CommandOpcode::WaitSyncObject, cmd_handle_wait_sync_object },
        { CommandOpcode::SignalNotification, cmd_handle_notification },
        { CommandOpcode::NewFrame, cmd_new_frame },
        { CommandOpcode::DestroyRenderTarget, cmd_handle_destroy_render_target },
        { CommandOpcode::DestroyContext, cmd_handle_destroy_context }
    };

    Command *cmd = command_list.first;

    // Take a batch, and execute it. Hope it's not too large
    do {
        if (cmd == nullptr) {
            break;
        }

        auto handler = handlers.find(cmd->opcode);
        if (handler == handlers.end()) {
            LOG_ERROR("Unimplemented command opcode {}", static_cast<int>(cmd->opcode));
        } else {
            CommandHelper helper(cmd);
            handler->second(state, mem, config, helper, features, command_list.context);
        }

        Command *last_cmd = cmd;
        cmd = cmd->next;

        if (command_list.context) {
            command_list.context->free_func(last_cmd);
        } else {
            generic_command_free(last_cmd);
        }
    } while (true);
}

void process_batches(renderer::State &state, const FeatureState &features, MemState &mem, Config &config) {
    // always display a frame every 500ms
    auto max_time = duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count() + 500;

    while (!state.should_display) {
        // Try to wait for a batch (about 2 or 3ms, game should be fast for this)
        auto cmd_list = state.command_buffer_queue.top(3);

        if (!cmd_list || !is_cmd_ready(mem, *cmd_list)) {
            // beginning of the game or homebrew not using gxm
            if (state.context == nullptr)
                return;

            // keep the old behavior for opengl with vsync as it looks like the new one causes some issues
            if (state.current_backend == Backend::OpenGL && config.current_config.v_sync)
                return;

            if (!cmd_list || !wait_cmd(mem, *cmd_list)) {
                auto curr_time = duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
                if (curr_time >= max_time)
                    // display a frame even though the game is not diplaying anything
                    return;

                // this mean the command is still not ready, check if we can display it again
                continue;
            }
        }

        state.command_buffer_queue.pop();
        process_batch(state, features, mem, config, *cmd_list);
    }
}

void reset_command_list(CommandList &command_list) {
    command_list.first = nullptr;
    command_list.last = nullptr;
}
} // namespace renderer
