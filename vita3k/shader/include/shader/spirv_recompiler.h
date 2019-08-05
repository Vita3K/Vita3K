#pragma once

#include <gxm/types.h>
#include <psp2/gxm.h>
#include <shader/usse_translator_types.h>
#include <shader/usse_types.h>

#include <features/state.h>

#include <string>
#include <utility>
#include <vector>

namespace spv {
class Builder;
}

namespace shader {

// Dump generated SPIR-V disassembly up to this point
void spirv_disasm_print(const usse::SpirvCode &spirv_binary, std::string *spirv_dump = nullptr);

std::string convert_gxp_to_glsl(const SceGxmProgram &program, const std::string &shader_hash, const FeatureState &features,
    bool force_shader_debug = false, std::string *spirv_dump = nullptr, std::string *disasm_dump = nullptr);

void convert_gxp_to_glsl_from_filepath(const std::string &shader_filepath);

} // namespace shader
