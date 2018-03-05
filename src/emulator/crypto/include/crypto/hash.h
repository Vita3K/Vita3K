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

#include <array>

using Sha256Hash = std::array<uint8_t, 32>;

Sha256Hash sha256(const void *data, size_t size);

template <size_t N>
constexpr std::array<char, (N * 2) + 1> hex(const std::array<uint8_t, N> &hash) {
    std::array<char, (N * 2) + 1> dst;
    
    const char hex[17] = "0123456789abcdef";
    for (size_t i = 0, j = 0; i < N; ++i) {
        const uint8_t byte = hash[i];
        const char hi = hex[byte >> 4];
        const char lo = hex[byte & 0xf];
        dst[j++] = hi;
        dst[j++] = lo;
    }
    
    dst.back() = '\0';
    
    return dst;
}
