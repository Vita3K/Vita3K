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

#include <shader/usse_types.h>

#include <fstream>
#include <memory>
#include <string>

namespace shader {
namespace usse {
namespace disasm {

extern std::string *disasm_storage;

//
// Disasm helpers
//

const std::string &opcode_str(const Opcode &e);
const char *e_predicate_str(ExtPredicate p);
const char *s_predicate_str(ShortPredicate p);
const char *data_type_str(DataType p);
std::string reg_to_str(RegisterBank bank, uint32_t reg_num);
std::string operand_to_str(Operand op, Imm4 write_mask, int32_t shift = 0);
template <std::size_t s>
std::string swizzle_to_str(Swizzle<s> swizz, const Imm4 write_mask);

} // namespace disasm
} // namespace usse
} // namespace shader

// TODO: make LOG_RAW
#define LOG_DISASM(fmt_str, ...)                                        \
    {                                                                   \
        auto fmt_disasm = fmt::format(fmt_str, ##__VA_ARGS__);          \
        std::cout << fmt_disasm << std::endl;                           \
        if (shader::usse::disasm::disasm_storage)                       \
            *shader::usse::disasm::disasm_storage += fmt_disasm + '\n'; \
    }
