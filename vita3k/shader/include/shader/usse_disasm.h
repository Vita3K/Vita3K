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
std::string operand_to_str(Operand op, Imm4 write_mask, uint32_t shift = 0);
template <std::size_t s>
std::string swizzle_to_str(Swizzle<s> swizz, const Imm4 write_mask, uint32_t shift = 0);

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
