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

#include <tuple>
#include "args_layout.h"

struct LayoutArgsState {
    size_t gpr_used;
    size_t stack_used;
};

constexpr size_t align(size_t current, size_t alignment) {
    return (current + (alignment - 1)) & ~(alignment - 1);
}

template <typename Arg>
constexpr std::tuple<ArgLayout, LayoutArgsState> add_to_stack(const LayoutArgsState &state) {
    const size_t stack_alignment = alignof(Arg); // TODO Assumes host matches ARM.
    const size_t stack_required = sizeof(Arg); // TODO Should this be aligned up?
    const size_t stack_offset = align(state.stack_used, stack_alignment);
    const size_t next_stack_used = stack_offset + stack_required;
    const ArgLayout layout = { ArgLocation::stack, stack_offset };
    const LayoutArgsState next_state = { state.gpr_used, next_stack_used };
    
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
    const LayoutArgsState next_state = { next_gpr_used, state.stack_used };
    
    return { layout, next_state };
}

template <typename Arg>
constexpr std::tuple<ArgLayout, LayoutArgsState> add_arg_to_layout(const LayoutArgsState &state) {
    // TODO Support floats and vectors.
    return add_to_gpr_or_stack<Arg>(state);
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
    add_args_to_layout<Tail...>(*(&head + 1), state);
}

template <typename... Args>
constexpr ArgsLayout<Args...> lay_out() {
    ArgsLayout<Args...> layout = {};
    LayoutArgsState state = {};
    add_args_to_layout<Args...>(*layout.data(), state);
    
    return layout;
}
