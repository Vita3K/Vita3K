// Vita3K emulator project
// Copyright (C) 2023 Vita3K team
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

#include <bit>
#include <cstring> // memcpy

#ifndef __cpp_lib_bit_cast
namespace std {
template <typename T, typename U>
T bit_cast(U &&u) {
    static_assert(sizeof(T) == sizeof(U));
    union {
        T t;
    }; // prevent construction
    std::memcpy(&t, &u, sizeof(t));
    return t;
}
} // namespace std
#endif