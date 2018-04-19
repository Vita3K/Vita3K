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
    return dst;
}

template <typename... Args>
constexpr ArgsLayout<Args...> lay_out() {
    return { };
}

template <typename Arg>
Arg read(CPUState &cpu, const ArgLayout &arg, const MemState &mem) {
    return Arg();
}

template <typename Arg, typename... Args>
Arg read(CPUState &cpu, const ArgsLayout<Args...> &args, size_t index, const MemState &mem) {
    return read<Arg>(cpu, args[index], mem);
}
