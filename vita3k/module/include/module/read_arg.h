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
#include <util/preprocessor.h>

namespace module {
class vargs;
}

// Read 32-bit (or smaller) values from a single register.
template <typename T>
typename std::enable_if<sizeof(T) <= 4, T>::type read_from_gpr(CPUState &cpu, const ArgLayout &arg) {
    const uint32_t reg = read_reg(cpu, arg.offset);
    return static_cast<T>(reg);
}

// Read 64-bit values from 2 registers.
template <typename T>
typename std::enable_if<sizeof(T) == 8, T>::type read_from_gpr(CPUState &cpu, const ArgLayout &arg) {
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
template <typename T, typename std::enable_if<!std::is_same<T, module::vargs>::value>::type * = nullptr>
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
        VITA3K_IF_CONSTEXPR(std::is_same<T, float>::value)
        return read_from_fp<T>(cpu, arg);
    }

    return T();
}

template <typename T, typename std::enable_if<std::is_same<T, module::vargs>::value>::type * = nullptr>
T read(CPUState &cpu, const ArgLayout &arg, const MemState &mem) {
    return T();
}

template <typename T>
T make_vargs(const LayoutArgsState &state);

template <typename Arg, size_t index, typename... Args>
Arg read(CPUState &cpu, const ArgsLayout<Args...> &args, const LayoutArgsState &state, const MemState &mem) {
    using ArmType = typename BridgeTypes<Arg>::ArmType;

    // Note (bentokun): The else block was intentionally made to workaround evaluation
    // fault where MSVC evaluates the rest of the function when the Arg type is vargs
    VITA3K_IF_CONSTEXPR(std::is_same<Arg, module::vargs>::value) {
        return make_vargs<Arg>(state);
    }
    else {
        const ArmType bridged = read<ArmType>(cpu, args[index], mem);
        return BridgeTypes<Arg>::arm_to_host(bridged, mem);
    }
}
