#pragma once

#include <gxm/types.h>
#include <psp2/gxm.h>
#include <shader/types.h>

#include <string>

namespace spv {
class Builder;
}

namespace shader {

std::string convert_gxp_to_glsl(const SceGxmProgram &program, const std::string &shader_hash, bool force_shader_debug = false);
void convert_gxp_to_glsl_from_filepath(const std::string &shader_filepath);

} // namespace shader
