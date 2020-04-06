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

#pragma once

#include <cassert>
#include <cstdint>
#include <vector>

struct GxmContextState;

namespace renderer {
#define REPORT_MISSING(backend) //LOG_ERROR("Unimplemented graphics API handler with backend {}", (int)backend)
#define REPORT_STUBBED() //LOG_INFO("Stubbed")

struct Context;
struct State;

enum class CommandOpcode : std::uint16_t {
    // These two functions are sync, and taking pointer as parameter.
    CreateContext = 0,
    CreateRenderTarget = 1,

    /**
         * Do draw.
         */
    Draw = 2,

    /**
         * This is like a NOP. It only signals back to client.
         */
    Nop = 3,

    /**
         * Set a GXM state.
         */
    SetState = 4,

    SetContext = 5,
    SyncSurfaceData = 6,

    /**
         * Jump to another command pointer, save current command pointer on a stack
         */
    JumpWithLink = 7,

    /**
         * Pop the stack, and jump the command popped
         */
    JumpBack = 8,

    /**
         * Signal sync object that fragment has been done.
         */
    SignalSyncObject = 9,

    DestroyRenderTarget = 10
};

enum CommandErrorCode {
    CommandErrorCodeNone = 0,
    CommandErrorCodePending = -1,
    CommandErrorArgumentsTooLarge = -2
};

constexpr std::size_t MAX_COMMAND_DATA_SIZE = 0x40;

struct Command {
    CommandOpcode opcode;
    std::uint8_t data[MAX_COMMAND_DATA_SIZE];
    int *status;

    Command *next;
};

using CommandPool = std::vector<Command>;

// This somehow looks like vulkan
// It's to split a command list easier when ExecuteCommandList is used.
struct CommandList {
    Command *first{ nullptr };
    Command *last{ nullptr };
    Context *context; ///< The HLE context that try to execute this buffer.
    GxmContextState *gxm_context; ///< The GXM context associated.
};

struct CommandHelper {
    Command *cmd;
    std::uint32_t point;

    explicit CommandHelper(Command *cmd)
        : cmd(cmd)
        , point(0) {
    }

    template <typename T>
    bool push(T &val) {
        if (point + sizeof(T) > MAX_COMMAND_DATA_SIZE) {
            return false;
        }

        *(T *)(cmd->data + point) = val;
        point += sizeof(T);

        return true;
    }

    template <typename T>
    T pop() {
        if (point + sizeof(T) > MAX_COMMAND_DATA_SIZE) {
            // Shouldn't happen
            assert(false);
        }

        T *data = (T *)(cmd->data + point);
        point += sizeof(T);

        return *data;
    }

    void complete(const int code) {
        *cmd->status = code;
    }
};

template <typename Head, typename... Args>
bool do_command_push_data(CommandHelper &helper, Head arg1, Args... args2) {
    if (!helper.push(arg1)) {
        return false;
    }

    if constexpr (sizeof...(args2) > 0) {
        return do_command_push_data(helper, args2...);
    }

    return true;
}

template <typename... Args>
Command *make_command(const CommandOpcode opcode, int *status, Args... arguments) {
    Command *new_command = new Command;
    new_command->opcode = opcode;
    new_command->status = status;
    new_command->next = nullptr;

    CommandHelper helper(new_command);

    if constexpr (sizeof...(arguments) > 0) {
        if (!do_command_push_data(helper, arguments...)) {
            delete new_command;
            return nullptr;
        }
    }

    return new_command;
}

void complete_command(State &state, CommandHelper &helper, const int code);
} // namespace renderer
