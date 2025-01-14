// Vita3K emulator project
// Copyright (C) 2025 Vita3K team
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
#include <cstring>
#include <type_traits>

#ifndef __cpp_lib_bit_cast
namespace std {
// https://en.cppreference.com/w/cpp/numeric/bit_cast
template <class To, class From>
std::enable_if_t<sizeof(To) == sizeof(From), To> bit_cast(const From &src) noexcept {
    if constexpr (alignof(From) >= alignof(To)) {
        return *reinterpret_cast<const To *>(&src);
    } else {
        alignas(alignof(To)) char dst[sizeof(To)];
        std::memcpy(&dst, &src, sizeof(To));
        return *reinterpret_cast<To *>(&dst);
    }
}
} // namespace std
#endif
