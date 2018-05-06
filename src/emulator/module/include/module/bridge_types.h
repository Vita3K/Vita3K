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

// By default, do no special conversion.
template <typename HostType>
struct BridgeTypes {
    typedef HostType ArmType;

    static HostType arm_to_host(const ArmType &t, const MemState &mem) {
        return t;
    }
};

// Convert from address in ARM register/memory to host pointer.
template <typename Pointee>
struct BridgeTypes<Pointee *> {
    typedef Ptr<Pointee> ArmType;

    static Pointee *arm_to_host(const ArmType &t, const MemState &mem) {
        return t.get(mem);
    }
};
