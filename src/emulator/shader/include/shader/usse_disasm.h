#pragma once

#include <shader/usse_types.h>

#include <string>

// TODO: make LOG_RAW
#define LOG_DISASM(fmt_str, ...) std::cout << fmt::format(fmt_str, ##__VA_ARGS__) << std::endl

namespace shader {
namespace usse {
namespace disasm {

//
// Disasm helpers
//

const std::string &opcode_str(const Opcode &e);
const char *e_predicate_str(ExtPredicate p);
const char *s_predicate_str(ShortPredicate p);
const char *move_data_type_str(MoveDataType p);
std::string reg_to_str(RegisterBank bank, uint32_t reg_num);
std::string operand_to_str(Operand op, Imm4 write_mask, uint32_t shift = 0);
template <std::size_t s>
std::string swizzle_to_str(Swizzle<s> swizz, const Imm4 write_mask, uint32_t shift = 0);

} // namespace disasm
} // namespace usse
} // namespace shader
