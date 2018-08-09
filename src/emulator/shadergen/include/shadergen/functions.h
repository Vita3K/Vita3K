#pragma once

#include <psp2/gxm.h>

#include <string>

namespace shadergen {
namespace spirv {

std::string generate_vertex_glsl(const SceGxmProgram &program);
std::string generate_fragment_glsl(const SceGxmProgram &program);

} // namespace spirv
} // namespace shadergen
