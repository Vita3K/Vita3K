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

#include <shader/types.h>

namespace shader::usse {
bool is_sub_opcode(Opcode test_op) {
    return (test_op == Opcode::VSUB) || (test_op == Opcode::VF16SUB) || (test_op == Opcode::ISUB8) || (test_op == Opcode::ISUB16) || (test_op == Opcode::ISUB32) || (test_op == Opcode::ISUBU8) || (test_op == Opcode::ISUBU16) || (test_op == Opcode::ISUBU32) || (test_op == Opcode::FPSUB8);
}
} // namespace shader::usse