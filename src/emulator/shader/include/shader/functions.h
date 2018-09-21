#pragma once

#include <gxm/types.h>
#include <psp2/gxm.h>

#include <string>

namespace shader {

std::string convert_gxp_to_glsl(const SceGxmProgram &program, emu::SceGxmProgramType program_type);

} // namespace shader
