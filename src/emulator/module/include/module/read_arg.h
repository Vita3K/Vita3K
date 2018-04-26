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

// Read variable from register.
template <typename T>
std::enable_if_t<sizeof(T) < 8, T> read_from_gpr(CPUState &cpu, const ArgLayout &arg) {
    const uint32_t reg = read_reg(cpu, arg.offset);
    return static_cast<T>(reg);
}

// Specialised for 64-bit reads, which span 2 registers.
template <typename T>
std::enable_if_t<sizeof(T) == 8, T> read_from_gpr(CPUState &cpu, const ArgLayout &arg) {
    const uint64_t lo32 = read_reg(cpu, arg.offset);
    const uint64_t hi32 = read_reg(cpu, arg.offset + 1);
    const uint64_t both = lo32 | (hi32 << 32);
    return both;
}

// Read variable from register or stack, as specified by arg layout.
template <typename T>
T read(CPUState &cpu, const ArgLayout &arg, const MemState &mem) {
    switch (arg.location) {
        case ArgLocation::gpr:
            return read_from_gpr<T>(cpu, arg);
        case ArgLocation::stack:
        {
            const Address sp = read_sp(cpu);
            const Address address_on_stack = sp + arg.offset;
            return *Ptr<T>(address_on_stack).get(mem);
        }
    }
}

template <typename Arg, size_t index, typename... Args>
Arg read(CPUState &cpu, const ArgsLayout<Args...> &args, const MemState &mem) {
    using ArmType = typename BridgeTypes<Arg>::ArmType;
    
    const ArmType bridged = read<ArmType>(cpu, args[index], mem);
    return BridgeTypes<Arg>::arm_to_host(bridged, mem);
}
