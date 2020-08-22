#pragma once

#include <gxm/types.h>
#include <shader/usse_translator_types.h>
#include <shader/usse_types.h>

namespace shader {

std::tuple<usse::DataType, std::string> get_parameter_type_store_and_name(const SceGxmParameterType &type);
std::vector<usse::UniformBuffer> get_uniform_buffers(const SceGxmProgram &program);
} // namespace shader