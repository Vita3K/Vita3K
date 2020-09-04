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

#include <gxm/functions.h>
#include <gxm/types.h>
#include <shader/gxp_parser.h>
#include <shader/usse_program_analyzer.h>

#include <cassert>

#include <shader/usse_types.h>

namespace shader::usse {
bool is_kill(const std::uint64_t inst) {
    return (inst >> 59) == 0b11111 && (((inst >> 32) & ~0xF8FFFFFF) >> 24) == 1 && ((inst >> 32) & ~0xFFCFFFFF) >> 20 == 3;
}

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
    // Special instructions
    case 0b11111: {
        if ((((inst >> 32) & ~0xFF8FFFFF) >> 20) == 0) {
            break;
        }

        // Kill
        if ((((inst >> 32) & ~0xF8FFFFFF) >> 24) == 1 && ((inst >> 32) & ~0xFFCFFFFF) >> 20 == 3) {
            uint8_t pred = ((inst >> 32) & 0xFFFFF9FF) >> 9;
            switch (pred) {
            case 0:
                return static_cast<uint8_t>(ExtPredicate::NONE);
            case 1:
                return static_cast<uint8_t>(ExtPredicate::P0);
            case 2:
                return static_cast<uint8_t>(ExtPredicate::P1);
            case 3:
                return static_cast<uint8_t>(ExtPredicate::NEGP0);
            default:
                return 0;
            }
        }

        return 0;
    }

    // VMAD normal version, predicates only occupied two bits
    case 0b00100:
    case 0b00101: {
        uint8_t predicate = ((inst >> 32) & ~0xFCFFFFFF) >> 24;
        switch (predicate) {
        case 0:
            return static_cast<uint8_t>(ExtPredicate::NONE);
        case 1:
            return static_cast<uint8_t>(ExtPredicate::P0);
        case 2:
            return static_cast<uint8_t>(ExtPredicate::NEGP0);
        case 3:
            return static_cast<uint8_t>(ExtPredicate::PN);
        default:
            return 0;
        }
    }
    // SOP2
    case 0b10000: {
        uint8_t predicate = ((inst >> 32) & ~0xF9FFFFFF) >> 25;
        return static_cast<uint8_t>(short_predicate_to_ext(static_cast<ShortPredicate>(predicate)));
    }
    case 0b00000:
        return ((inst >> 32) & ~0xFCFFFFFF) >> 24;

    default:
        break;
    }

    return ((inst >> 32) & ~0xF8FFFFFFU) >> 24;
}

bool is_buffer_fetch_or_store(const std::uint64_t inst, int &base, int &cursor, int &offset, int &size) {
    // TODO: Is there any exception? Like any instruction use pre or post increment addressing mode.
    cursor = 0;

    // Are you me? Or am i you
    if (((inst >> 59) & 0b11111) == 0b11101 || ((inst >> 59) & 0b11111) == 0b11110) {
        // Get the base
        offset = cursor + (inst >> 7) & 0b1111111;
        base = (inst >> 14) & 0b1111111;

        // Data type downwards: 4 bytes, 2 bytes, 1 bytes, 0 bytes
        // Total to fetch is at bit 44, and last for 4 bits.
        size = 4 / (((inst >> 36) & 0b11) + 1) * (((inst >> 44) & 0b1111) + 1);

        return true;
    }

    return false;
}

int match_uniform_buffer_with_buffer_size(const SceGxmProgram &program, const SceGxmProgramParameter &parameter, const shader::usse::UniformBufferMap &buffers) {
    assert(parameter.component_count == 0 && parameter.category == SCE_GXM_PARAMETER_CATEGORY_UNIFORM_BUFFER);

    // Determine base value
    int base = gxp::get_uniform_buffer_base(program, parameter);

    // Search for the buffer from analyzed list
    if (buffers.find(base) != buffers.end()) {
        return (buffers.at(base).size + 3) / 4;
    }

    return -1;
}

void get_uniform_buffer_sizes(const SceGxmProgram &program, UniformBufferSizes &sizes) {
    const auto program_input = shader::get_program_input(program);
    for (const auto buffer : program_input.uniform_buffers) {
        uint32_t index = buffer.index;
        if (index == 0) {
            index = 14;
        } else {
            --index;
        }
        sizes[index] = buffer.size;
    }
}
} // namespace shader::usse
