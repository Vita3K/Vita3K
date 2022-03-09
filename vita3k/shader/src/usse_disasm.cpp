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

#include <shader/usse_disasm.h>

#include <shader/usse_types.h>

#include <string>
#include <unordered_map>

using namespace shader::usse;

namespace shader::usse::disasm {

std::string *disasm_storage = nullptr;

//
// Disasm helpers
//

const std::string &opcode_str(const Opcode &e) {
    static const std::unordered_map<Opcode, const std::string> names = {
#define OPCODE(n) { Opcode::n, std::string(#n) },
#include "shader/usse_opcodes.inc"
#undef OPCODE
    };
    return names.at(e);
}

const char *e_predicate_str(ExtPredicate p) {
    switch (p) {
    case ExtPredicate::NONE: return "";
    case ExtPredicate::P0: return "p0 ";
    case ExtPredicate::P1: return "p1 ";
    case ExtPredicate::P2: return "p2 ";
    case ExtPredicate::P3: return "p3 ";
    case ExtPredicate::NEGP0: return "!p0 ";
    case ExtPredicate::NEGP1: return "!p1 ";
    case ExtPredicate::PN: return "pN ";
    default: return "invalid";
    }
}
const char *s_predicate_str(ShortPredicate p) {
    switch (p) {
    case ShortPredicate::NONE: return "";
    case ShortPredicate::P0: return "p0 ";
    case ShortPredicate::P1: return "p1 ";
    case ShortPredicate::NEGP0: return "!p0 ";
    default: return "invalid";
    }
}

const char *data_type_str(DataType p) {
    switch (p) {
    case DataType::INT8: return "i8";
    case DataType::INT16: return "i16";
    case DataType::INT32: return "i32";
    case DataType::UINT8: return "u8";
    case DataType::UINT16: return "u16";
    case DataType::UINT32: return "u32";
    case DataType::C10: return "c10";
    case DataType::F16: return "f16";
    case DataType::F32: return "f32";
    default: return "invalid";
    }
}

std::string reg_to_str(RegisterBank bank, uint32_t reg_num) {
    std::string opstr;

    switch (bank) {
    case RegisterBank::PRIMATTR: {
        opstr += "pa";
        break;
    }

    case RegisterBank::SECATTR: {
        opstr += "sa";
        break;
    }

    case RegisterBank::TEMP: {
        opstr += "r";
        break;
    }

    case RegisterBank::OUTPUT: {
        opstr += "o";
        break;
    }

    case RegisterBank::FPINTERNAL: {
        opstr += "i";
        break;
    }

    case RegisterBank::FPCONSTANT: {
        opstr += "c";
        break;
    }

    case RegisterBank::INDEX: {
        opstr += "idx";
        break;
    }

    case RegisterBank::INDEXED1:
    case RegisterBank::INDEXED2: {
        // Decode the info. Usually the number of bits in a INDEXED number is 7.
        // TODO: Fix the assumption
        const Imm2 bank_enc = (reg_num >> 5) & 0b11;
        const Imm5 add_off = reg_num & 0b11111;

        switch (bank_enc) {
        case 0: {
            opstr += "r[";
            break;
        }

        case 1: {
            opstr += "o[";
            break;
        }

        case 2: {
            opstr += "pa[";
            break;
        }

        case 3: {
            opstr += "sa[";
            break;
        }

        default: {
            opstr += "inv[";
            break;
        }
        }

        opstr += "idx" + std::to_string((int)bank - (int)RegisterBank::INDEXED1 + 1) + " * 2 + " + std::to_string(add_off) + "]";

        break;
    }

    case RegisterBank::IMMEDIATE: {
        break;
    }

    default: {
        opstr += "INVALID";
        break;
    }
    }

    if (bank != RegisterBank::INDEXED1 && bank != RegisterBank::INDEXED2) {
        opstr += std::to_string(reg_num);
    }

    return opstr;
}

std::string operand_to_str(Operand op, Imm4 write_mask, int32_t shift) {
    std::string opstr = reg_to_str(op.bank, op.num + shift);

    if (op.bank == RegisterBank::IMMEDIATE) {
        return opstr;
    }

    if (write_mask != 0) {
        opstr += "." + swizzle_to_str<4>(op.swizzle, write_mask);
    }

    if (op.flags & RegisterFlags::Negative) {
        opstr = "-" + opstr;
    }

    if (op.flags & RegisterFlags::Absolute) {
        opstr = "abs(" + opstr + ")";
    }

    return opstr;
}

template <std::size_t s>
std::string swizzle_to_str(Swizzle<s> swizz, Imm4 write_mask) {
    std::string swizzstr;

    for (std::size_t i = 0; i < s; i++) {
        if (write_mask & (1 << i)) {
            switch (swizz[i]) {
            case SwizzleChannel::C_X: {
                swizzstr += "x";
                break;
            }
            case SwizzleChannel::C_Y: {
                swizzstr += "y";
                break;
            }
            case SwizzleChannel::C_Z: {
                swizzstr += "z";
                break;
            }
            case SwizzleChannel::C_W: {
                swizzstr += "w";
                break;
            }
            case SwizzleChannel::C_0: {
                swizzstr += "0";
                break;
            }
            case SwizzleChannel::C_1: {
                swizzstr += "1";
                break;
            }
            case SwizzleChannel::C_2: {
                swizzstr += "2";
                break;
            }
            case SwizzleChannel::C_H: {
                swizzstr += "H";
                break;
            }
            default: {
                swizzstr += "?";
                break;
            }
            }
        } else {
            // Not available
            swizzstr += "-";
        }
    }

    // Erase all - at the end
    while (!swizzstr.empty() && swizzstr.back() == '-')
        swizzstr.pop_back();

    return swizzstr;
}

} // namespace shader::usse::disasm
