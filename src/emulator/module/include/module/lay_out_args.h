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

#include "args_layout.h"

#include <util/align.h>

#include <tuple>

template <typename Arg>
constexpr std::tuple<ArgLayout, LayoutArgsState> add_to_stack(const LayoutArgsState &state) {
    const size_t stack_alignment = alignof(Arg); // TODO Assumes host matches ARM.
    const size_t stack_required = sizeof(Arg); // TODO Should this be aligned up?
    const size_t stack_offset = align(state.stack_used, stack_alignment);
    const size_t next_stack_used = stack_offset + stack_required;
    const ArgLayout layout = { ArgLocation::stack, stack_offset };
    const LayoutArgsState next_state = { state.gpr_used, next_stack_used, state.float_used };

    return { layout, next_state };
}

template <typename Arg>
constexpr std::tuple<ArgLayout, LayoutArgsState> add_to_gpr_or_stack(const LayoutArgsState &state) {
    const size_t gpr_required = (sizeof(Arg) + 3) / 4;
    const size_t gpr_alignment = gpr_required;
    const size_t gpr_index = align(state.gpr_used, gpr_alignment);
    const size_t next_gpr_used = gpr_index + gpr_required;

    // Does variable not fit in register file?
    if (next_gpr_used > 4) {
        return add_to_stack<Arg>(state);
    }

    // Lay out in registers.
    const ArgLayout layout = { ArgLocation::gpr, gpr_index };
    const LayoutArgsState next_state = { next_gpr_used, state.stack_used, state.float_used };

    return { layout, next_state };
}

template <typename Arg>
constexpr std::tuple<ArgLayout, LayoutArgsState> add_to_float(const LayoutArgsState &state) {
    const size_t float_index = state.float_used;
    const size_t next_float_used = state.float_used + 1;

    // Lay out in registers.
    const ArgLayout layout = { ArgLocation::fp, float_index };
    const LayoutArgsState next_state = { state.gpr_used, state.stack_used, next_float_used };

    return { layout, next_state };
}

template <typename Arg>
constexpr std::tuple<ArgLayout, LayoutArgsState> add_arg_to_layout(const LayoutArgsState &state) {
    // TODO Support floats and vectors.
    if constexpr (std::is_same_v<Arg, float>) {
        return add_to_float<Arg>(state);
    } else {
        return add_to_gpr_or_stack<Arg>(state);
    }
}

// Empty argument list -- no arguments to add.
template <typename... Args>
constexpr std::enable_if_t<sizeof...(Args) == 0> add_args_to_layout(ArgLayout &head, LayoutArgsState &state) {
    // Nothing to do.
}

// One or more arguments to add.
template <typename Head, typename... Tail>
constexpr void add_args_to_layout(ArgLayout &head, LayoutArgsState &state) {
    // Add the argument at the head of the list.
    const std::tuple<ArgLayout, LayoutArgsState> result = add_arg_to_layout<Head>(state);
    head = std::get<0>(result);
    state = std::get<1>(result);

    // Recursively add the remaining arguments.
    if (sizeof...(Tail) > 0) {
        *(&state + 1) = state;
        add_args_to_layout<Tail...>(*(&head + 1), *(&state + 1));
    }
}

template <typename... Args>
constexpr std::tuple<ArgsLayout<Args...>, LayoutArgsState> lay_out() {
    std::array<LayoutArgsState, sizeof...(Args)> states = {};
    ArgsLayout<Args...> layout = {};

    if (sizeof...(Args) > 0) {
        add_args_to_layout<Args...>(layout[0], states[0]);
    }

    if (sizeof...(Args) <= 1) {
        LayoutArgsState state {0, 0, 0};
        return { layout, state };
    } else {
        return { layout, states[sizeof...(Args) - 2] };
    }
}
