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
#include "bridge_types.h"

#include <cpu/functions.h>

namespace module {
    class vargs;
}

// Read 32-bit (or smaller) values from a single register.
template <typename T>
std::enable_if_t<sizeof(T) <= 4, T> read_from_gpr(CPUState &cpu, const ArgLayout &arg) {
    const uint32_t reg = read_reg(cpu, arg.offset);
    return static_cast<T>(reg);
}

// Read 64-bit values from 2 registers.
template <typename T>
std::enable_if_t<sizeof(T) == 8, T> read_from_gpr(CPUState &cpu, const ArgLayout &arg) {
    const uint64_t lo32 = read_reg(cpu, arg.offset);
    const uint64_t hi32 = read_reg(cpu, arg.offset + 1);
    const uint64_t both = lo32 | (hi32 << 32);
    return static_cast<T>(both);
}

// Read float value from register.
template <typename T>
T read_from_fp(CPUState &cpu, const ArgLayout &arg) {
    const float reg = read_float_reg(cpu, arg.offset);
    return static_cast<T>(reg);
}

// Read variable from register or stack, as specified by arg layout.
template <typename T>
T read(CPUState &cpu, const ArgLayout &arg, const MemState &mem) {
    switch (arg.location) {
    case ArgLocation::gpr:
        return read_from_gpr<T>(cpu, arg);
    case ArgLocation::stack: {
        const Address sp = read_sp(cpu);
        const Address address_on_stack = static_cast<Address>(sp + arg.offset);
        return *Ptr<T>(address_on_stack).get(mem);
    }
    case ArgLocation::fp:
        if constexpr (std::is_same_v<T, float>)
            return read_from_fp<T>(cpu, arg);
    }

    return T();
}

template <typename T>
T make_vargs(const LayoutArgsState &state);

template <typename Arg, size_t index, typename... Args>
Arg read(CPUState &cpu, const std::tuple<ArgsLayout<Args...>, LayoutArgsState> &args, const MemState &mem) {
    using ArmType = typename BridgeTypes<Arg>::ArmType;

    // Note (bentokun): The else block was intentionally made to workaround evaluation
    // fault where MSVC evaluates the rest of the function when the Arg type is vargs
    if constexpr(std::is_same_v<Arg, module::vargs>) {
        LayoutArgsState layoutState = std::get<1>(args);
        return make_vargs<Arg>(layoutState);
    } else {
        const ArmType bridged = read<ArmType>(cpu, std::get<0>(args)[index], mem);
        return BridgeTypes<Arg>::arm_to_host(bridged, mem);
    }
}