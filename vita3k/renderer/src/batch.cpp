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

#include <renderer/commands.h>
#include <renderer/functions.h>
#include <renderer/state.h>
#include <renderer/types.h>

#include "driver_functions.h"

#include <functional>
#include <util/log.h>
#include <util/string_utils.h>

struct FeatureState;

namespace renderer {
Command *generic_command_allocate() {
    return new Command;
}

void generic_command_free(Command *cmd) {
    delete cmd;
}

void complete_command(State &state, CommandHelper &helper, const int code) {
    helper.complete(code);
    state.command_finish_one.notify_all();
}

void process_batch(renderer::State &state, const FeatureState &features, MemState &mem, Config &config, CommandList &command_list, const char *base_path,
    const char *title_id) {
    using CommandHandlerFunc = std::function<void(renderer::State &, MemState &, Config &,
        CommandHelper &, const FeatureState &, Context *, const char *, const char *)>;

    static std::map<CommandOpcode, CommandHandlerFunc> handlers = {
        { CommandOpcode::SetContext, cmd_handle_set_context },
        { CommandOpcode::SyncSurfaceData, cmd_handle_sync_surface_data },
        { CommandOpcode::CreateContext, cmd_handle_create_context },
        { CommandOpcode::CreateRenderTarget, cmd_handle_create_render_target },
        { CommandOpcode::Draw, cmd_handle_draw },
        { CommandOpcode::Nop, cmd_handle_nop },
        { CommandOpcode::SetState, cmd_handle_set_state },
        { CommandOpcode::SignalSyncObject, cmd_handle_signal_sync_object },
        { CommandOpcode::SignalNotification, cmd_handle_notification },
        { CommandOpcode::DestroyRenderTarget, cmd_handle_destroy_render_target }
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
            handler->second(state, mem, config, helper, features, command_list.context, base_path, title_id);
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

void process_batches(renderer::State &state, const FeatureState &features, MemState &mem, Config &config, const char *base_path,
    const char *title_id) {
    std::uint32_t processed_count = 0;

    while (processed_count < state.average_scene_per_frame) {
        auto cmd_list = state.command_buffer_queue.pop(2);

        if (!cmd_list) {
            // Try to wait for a batch (about 1 or 2ms, game should be fast for this)
            return;
        }

        process_batch(state, features, mem, config, *cmd_list, base_path, title_id);
        processed_count++;
    }
}

void reset_command_list(CommandList &command_list) {
    command_list.first = nullptr;
    command_list.last = nullptr;
}
} // namespace renderer
