#pragma once

#include <gxm/types.h>
#include <psp2/gxm.h>

#include <string>

namespace shadergen {
namespace spirv {

std::string generate_shader_glsl(const SceGxmProgram &program, emu::SceGxmProgramType program_type);

} // namespace spirv
} // namespace shadergen
