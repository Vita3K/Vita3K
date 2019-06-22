// Vita3K emulator project
// Copyright (C) 2019 Vita3K team
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

#include <shader/usse_program_analyzer.h>

namespace shader::usse {
bool is_branch(const std::uint64_t inst, std::uint8_t &pred, std::uint32_t &br_off) {
    const std::uint32_t high = (inst >> 32);
    const std::uint32_t low = static_cast<std::uint32_t>(inst);

    const bool br_inst_is = ((high & ~0x07FFFFFFU) >> 27 == 0b11111) && (((high & ~0xFFCFFFFFU) >> 20) == 0) && !(high & 0x00400000) && ((((high & ~0xFFFFFE3FU) >> 6) == 0) || ((high & ~0xFFFFFE3FU) >> 6) == 1);

    if (br_inst_is) {
        br_off = (low & ((1 << 20) - 1));
        pred = (high & ~0xF8FFFFFFU) >> 24;
    }

    return br_inst_is;
}

bool does_write_to_predicate(const std::uint64_t inst, std::uint8_t &pred) {
    if ((((inst >> 59) & 0b11111) == 0b01001) || (((inst >> 59) & 0b11111) == 0b01111)) {
        pred = static_cast<std::uint8_t>((inst & ~0xFFFFFFF3FFFFFFFF) >> 34);
        return true;
    }

    return false;
}

std::uint8_t get_predicate(const std::uint64_t inst) {
    switch (inst >> 59) {
    // Special instructions, no predicate
    case 0b11111: {
        if ((((inst >> 32) & ~0xFF8FFFFF) >> 20) == 0) {
            break;
        }

        return 0;
    }

    // VMAD normal version, predicates only occupied two bits
    case 0b00000:
        return ((inst >> 32) & ~0xFCFFFFFF) >> 24;

    default:
        break;
    }

    return ((inst >> 32) & ~0xF8FFFFFFU) >> 24;
}
} // namespace shader::usse
