
#include <shader/usse_disasm.h>

#include <shader/usse_types.h>

#include <string>
#include <unordered_map>

using namespace shader::usse;

namespace shader::usse::disasm {

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

const char *move_data_type_str(MoveDataType p) {
    switch (p) {
    case MoveDataType::INT8: return "i8";
    case MoveDataType::INT16: return "i16";
    case MoveDataType::INT32: return "i32";
    case MoveDataType::C10: return "c10";
    case MoveDataType::F16: return "f16";
    case MoveDataType::F32: return "f32";
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

    default: {
        opstr += "INVALID";
        break;
    }
    }

    opstr += std::to_string(reg_num);
    return opstr;
}

std::string operand_to_str(Operand op, Imm4 write_mask) {
    std::string opstr = reg_to_str(op.bank, op.num);

    if (write_mask != 0) {
        opstr += "." + swizzle_to_str<4>(op.swizzle, write_mask);
    }

    return opstr;
}

} // namespace shader::usse::disasm
