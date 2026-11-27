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

#include <module/call_result.h>

#include <bit>

CallResult call_result(int32_t ret) {
    return { .regs = { static_cast<uint32_t>(ret) }, .count = 1 };
}

CallResult call_result(int64_t ret) {
    return call_result(static_cast<uint64_t>(ret));
}

CallResult call_result(uint32_t ret) {
    return { .regs = { ret }, .count = 1 };
}

CallResult call_result(uint64_t ret) {
    return { .regs = { static_cast<uint32_t>(ret), static_cast<uint32_t>(ret >> 32) }, .count = 2 };
}

CallResult call_result(std::div_t ret) {
    return { .regs = { static_cast<uint32_t>(ret.quot), static_cast<uint32_t>(ret.rem) }, .count = 2 };
}

CallResult call_result(std::lldiv_t ret) {
    const auto quot = static_cast<uint64_t>(ret.quot);
    const auto rem = static_cast<uint64_t>(ret.rem);
    return { .regs = { static_cast<uint32_t>(quot), static_cast<uint32_t>(quot >> 32), static_cast<uint32_t>(rem), static_cast<uint32_t>(rem >> 32) }, .count = 4 };
}

CallResult call_result(bool ret) {
    return { .regs = { ret }, .count = 1 };
}

CallResult call_result(float ret) {
    return { .regs = { std::bit_cast<uint32_t>(ret) }, .count = 1, .in_s0 = true };
}
