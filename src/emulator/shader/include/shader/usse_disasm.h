#pragma once

#include <shader/types.h>

#include <string>

// TODO: make LOG_RAW
#define LOG_DISASM(fmt_str, ...) std::cout << fmt::format(fmt_str, ##__VA_ARGS__) << std::endl

//translator
namespace shader {
namespace usse {
namespace disasm {

//
// Disasm helpers
//

const std::string &opcode_str(const USSE::Opcode &e);
const char *e_predicate_str(USSE::ExtPredicate p);
const char *s_predicate_str(USSE::ShortPredicate p);
const char *move_data_type_str(USSE::MoveDataType p);
std::string reg_to_str(USSE::RegisterBank bank, uint32_t reg_num);
std::string operand_to_str(USSE::Operand op, USSE::Imm4 write_mask);

template <std::size_t s>
std::string swizzle_to_str(USSE::Swizzle<s> swizz, const USSE::Imm4 write_mask) {
    std::string swizzstr;

    for (std::size_t i = 0; i < s, write_mask & (1 << i); i++) {
        switch (swizz[i]) {
        case USSE::SwizzleChannel::_X: {
            swizzstr += "x";
            break;
        }
        case USSE::SwizzleChannel::_Y: {
            swizzstr += "y";
            break;
        }
        case USSE::SwizzleChannel::_Z: {
            swizzstr += "z";
            break;
        }
        case USSE::SwizzleChannel::_W: {
            swizzstr += "w";
            break;
        }
        case USSE::SwizzleChannel::_0: {
            swizzstr += "0";
            break;
        }
        case USSE::SwizzleChannel::_H: {
            swizzstr += "H";
            break;
        }
        default: break;
        }
    }

    return swizzstr;
}

} // namespace disasm
} // namespace usse
} // namespace shader
