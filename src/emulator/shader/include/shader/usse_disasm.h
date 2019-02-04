#pragma once

#include <shader/types.h>

#include <string>

// TODO: make LOG_RAW
#define LOG_DISASM(fmt_str, ...) std::cout << fmt::format(fmt_str, __VA_ARGS__) << std::endl

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

} // namespace disasm
} // namespace usse
} // namespace shader
