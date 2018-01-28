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

#include <mem/ptr.h>

struct CPUState;

template <typename Ret>
void bridge_return(CPUState &, Ret ret);

// Simple case - return value can be written directly to registers.
template <typename Ret>
struct BridgeReturn {
    static void write(CPUState &cpu, Ret ret) {
        bridge_return(cpu, ret);
    }
};

// Write pointers as addresses.
template <typename Pointee>
struct BridgeReturn<Ptr<Pointee>> {
    static void write(CPUState &cpu, const Ptr<Pointee> &ret) {
        bridge_return(cpu, ret.address());
    }
};
