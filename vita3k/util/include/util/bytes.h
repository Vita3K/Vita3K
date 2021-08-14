// Vita3K emulator project
// Copyright (C) 2021 Vita3K team
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

#include <cstdint>
#include <vector>

template <typename T>
T byte_swap(T val);

enum EndianType {
    LITTLE = 0x00000001,
    BIG = 0x01000000
};

/**
 * \brief Get system endian type.
 * 
 * Can be use at both compile-time and run-time. Only supports little and big endian.
 */
constexpr EndianType get_system_endian_type() {
    return ((0xFFFFFFFF & 0x1) == LITTLE) ? LITTLE : BIG;
}

template <typename T>
T network_to_host_order(T val) {
    if (get_system_endian_type() == EndianType::BIG) {
        return val;
    }

    return byte_swap(val);
}

void float_to_half(const float *src, std::uint16_t *dest, const int total);