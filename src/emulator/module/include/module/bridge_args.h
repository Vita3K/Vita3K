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

#include <cpu/functions.h>
#include <mem/ptr.h>

#include <array>

enum class ArgLocation {
    gpr,
    stack,
};

struct ArgLayout {
    ArgLocation location;
    size_t offset = 0;
};

struct LayoutArgsState {
    size_t gpr_used = 0;
    size_t stack_used = 0;
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
constexpr std::tuple<ArgLayout, LayoutArgsState> add(const LayoutArgsState &state) {
    return add_to_gpr_or_stack<Arg>(state);
}

template <typename... Args>
using ArgsLayout = std::array<ArgLayout, sizeof...(Args)>;

template <typename... Args>
constexpr ArgsLayout<Args...> lay_out() {
    // TODO
    return { };
}

// Read variable from register or stack, as specified by arg layout.
template <typename T>
T read(CPUState &cpu, const ArgLayout &arg, const MemState &mem) {
    switch (arg.location) {
        case ArgLocation::gpr:
        {
            const uint32_t reg = read_reg(cpu, arg.offset);
            return static_cast<T>(reg);
        }
        case ArgLocation::stack:
        {
            const Address sp = read_sp(cpu);
            const Address address_on_stack = sp + arg.offset;
            return *Ptr<T>(address_on_stack).get(mem);
        }
    }
}

// Arg in ARM register/memory requires no special conversion.
template <typename Arg>
struct BridgedArg {
    typedef Arg Type;
    
    static Arg bridge(const Type &t, const MemState &mem) {
        return t;
    }
};

// Convert from address in ARM register/memory to host pointer.
template <typename Pointee>
struct BridgedArg<Pointee *> {
    typedef Ptr<Pointee> Type;
    
    static Pointee *bridge(const Type &t, const MemState &mem) {
        return t.get(mem);
    }
};

template <typename Arg, typename... Args>
Arg read(CPUState &cpu, const ArgsLayout<Args...> &args, size_t index, const MemState &mem) {
    typedef typename BridgedArg<Arg>::Type BridgedType;
    
    const BridgedType bridged = read<BridgedType>(cpu, args[index], mem);
    return BridgedArg<Arg>::bridge(bridged, mem);
}
