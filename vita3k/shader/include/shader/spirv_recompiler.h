#pragma once

#include <gxm/types.h>
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
static constexpr int COLOR_ATTACHMENT_TEXTURE_SLOT_IMAGE = 0;
static constexpr int MASK_TEXTURE_SLOT_IMAGE = 1;

// Dump generated SPIR-V disassembly up to this point
void spirv_disasm_print(const usse::SpirvCode &spirv_binary, std::string *spirv_dump = nullptr);

usse::SpirvCode convert_gxp_to_spirv(const SceGxmProgram &program, const std::string &shader_hash, const FeatureState &features, const std::vector<SceGxmVertexAttribute> *hint_attributes = nullptr, bool maskupdate = false,
    bool force_shader_debug = false, std::function<bool(const std::string &ext, const std::string &dump)> dumper = nullptr);

std::string convert_gxp_to_glsl(const SceGxmProgram &program, const std::string &shader_hash, const FeatureState &features,
    const std::vector<SceGxmVertexAttribute> *hint_attributes = nullptr, bool maskupdate = false, bool force_shader_debug = false, std::function<bool(const std::string &ext, const std::string &dump)> dumper = nullptr);
void convert_gxp_to_glsl_from_filepath(const std::string &shader_filepath);

} // namespace shader
