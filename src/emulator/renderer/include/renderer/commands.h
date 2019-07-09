#pragma once

#include <cstdint>
#include <queue>

#include <boost/optional.hpp>

namespace renderer {
    enum CommandOpcode {
        BEGIN_SCENE = 0,
        END_SCENE = 1,
        SYNC_STATE = 2,
        DRAW = 3
    };

    enum CommandError {
        COMMAND_RESULT_NO_ERROR = 0,
        COMMAND_RESULT_ARGUMENT_INVALID = -1
    };

    constexpr std::size_t MAX_COMMAND_ARGUMENTS = 10;

    struct Command {
        CommandOpcode opcode;
        std::uint64_t arguments[MAX_COMMAND_ARGUMENTS];
        std::size_t argument_count;

        int *status;

        template <typename T>
        T get(const std::size_t index) {
            return (T)arguments[index];
        }

        void complete(const int result_code) {
            *status = result_code;
        }
    };

    template <typename ...Args>
    boost::optional<Command> make_command(const CommandOpcode opcode, int *status, Args... arguments) {
        if (sizeof...(Args) > MAX_COMMAND_ARGUMENTS) {
            return boost::none;
        }
        
        Command new_command;
        new_command.opcode = opcode;
        new_command.argument_count = sizeof...(Args);
        new_command.status = status;

        for (std::size_t i = 0; i < sizeof...(Args); i++) {
            new_command.arguments[i] = (std::uint64_t)arguments[i];
        }

        return new_command;
    }
}