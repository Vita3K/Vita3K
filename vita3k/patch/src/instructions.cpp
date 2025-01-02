// Vita3K emulator project
// Copyright (C) 2024 Vita3K team
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

#include "patch/instructions.h"

#include <util/log.h>

uint32_t nop(std::vector<uint32_t> &args) {
    return 0;
}

uint32_t t1_mov(std::vector<uint32_t> &args) {
    uint8_t b0 = args[1];
    uint8_t b1 = 0b00100000 | (args[0] << 8);

    return (b1 << 8) | b0;
}
