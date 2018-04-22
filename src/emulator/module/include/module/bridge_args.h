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

struct ArgLayout {
    size_t gpr = 0;
    size_t stack_offset = 0;
};

template <typename... Args>
using ArgsLayout = std::array<ArgLayout, sizeof...(Args)>;

template <typename Arg>
constexpr ArgLayout add(const ArgLayout &src) {
    const ArgLayout dst = src;
    // TODO
    return dst;
}

template <typename... Args>
constexpr ArgsLayout<Args...> lay_out() {
    // TODO
    return { };
}

template <typename T>
T read(CPUState &cpu, const ArgLayout &arg, const MemState &mem) {
    if (arg.gpr < 4) {
        const uint32_t reg = read_reg(cpu, arg.gpr);
        return static_cast<T>(reg);
    } else {
        const Address sp = read_sp(cpu);
        const Address address_on_stack = sp + arg.stack_offset;
        return *Ptr<T>(address_on_stack).get(mem);
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
