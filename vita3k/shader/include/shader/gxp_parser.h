#pragma once

#include <gxm/types.h>
#include <shader/usse_translator_types.h>
#include <shader/usse_types.h>

namespace shader {

usse::GenericType translate_generic_type(const gxp::GenericParameterType &type);
std::tuple<usse::DataType, std::string> get_parameter_type_store_and_name(const SceGxmParameterType &type);
std::vector<usse::UniformBuffer> get_uniform_buffers(const SceGxmProgram &program);
usse::ProgramInput get_program_input(const SceGxmProgram &program);
} // namespace shader