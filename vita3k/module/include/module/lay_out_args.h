// Vita3K emulator project
// Copyright (C) 2026 Vita3K team
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

#include "args_layout.h"

#include <util/align.h>

#include <tuple>

namespace module {
class vargs;
}

template <typename Arg>
constexpr std::tuple<ArgLayout, LayoutArgsState> add_arg_to_layout(const LayoutArgsState &state) {
    if constexpr (std::is_same_v<Arg, float>) {
        return { { ArgLocation::fp, state.float_used }, { state.gpr_used, state.stack_used, state.float_used + 1 } };
    } else {
        const std::size_t gpr_required = (sizeof(Arg) + 3) / 4;
        const std::size_t gpr_index = align(state.gpr_used, gpr_required);

        // Does variable not fit in register file?
        if (gpr_index + gpr_required > 4) {
            const std::size_t stack_alignment = alignof(Arg); // TODO Assumes host matches ARM.
            const std::size_t stack_required = sizeof(Arg); // TODO Should this be aligned up?
            const std::size_t stack_offset = align(state.stack_used, stack_alignment);
            return { { ArgLocation::stack, stack_offset }, { 4, stack_offset + stack_required, state.float_used } };
        }

        return { { ArgLocation::gpr, gpr_index }, { gpr_index + gpr_required, state.stack_used, state.float_used } };
    }
}

template <typename... Args>
consteval std::tuple<ArgsLayout<Args...>, LayoutArgsState> lay_out() {
    LayoutArgsState state{};
    ArgsLayout<Args...> layout{};
    std::size_t i = 0;

    auto step = [&]<typename Arg>() {
        if constexpr (!std::is_same_v<Arg, module::vargs>) {
            auto [l, s] = add_arg_to_layout<Arg>(state);
            layout[i++] = l;
            state = s;
        }
    };
    (step.template operator()<Args>(), ...);

    return { layout, state };
}
