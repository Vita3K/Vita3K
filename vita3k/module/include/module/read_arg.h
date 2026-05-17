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

#include <cpu/functions.h>

// Reads an arg from CPU registers or stack
template <typename T>
T read(CPUState &cpu, const ArgLayout &arg, const MemState &mem) {
    switch (arg.location) {
    case ArgLocation::gpr:
        if constexpr (sizeof(T) <= 4) {
            return static_cast<T>(read_reg(cpu, arg.offset));
        } else {
            static_assert(sizeof(T) == 8);
            const uint64_t lo = read_reg(cpu, arg.offset);
            const uint64_t hi = read_reg(cpu, arg.offset + 1);
            return static_cast<T>(lo | (hi << 32));
        }
    case ArgLocation::stack:
        return *Ptr<T>(static_cast<Address>(read_sp(cpu) + arg.offset)).get(mem);
    case ArgLocation::fp:
        if constexpr (std::is_same_v<T, float>) {
            return read_float_reg(cpu, arg.offset);
        }
    }
    return T();
}
