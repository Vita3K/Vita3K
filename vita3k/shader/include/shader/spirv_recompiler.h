// Vita3K emulator project
// Copyright (C) 2023 Vita3K team
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License along
// with this program; if not, write to the Free Software Foundation, Inc.,
// 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

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
static constexpr int COLOR_ATTACHMENT_RAW_TEXTURE_SLOT_IMAGE = 3;
static constexpr uint32_t CURRENT_VERSION = 8;

struct RenderVertUniformBlock {
    std::array<float, 4> viewport_flip;
    float viewport_flag;
    float screen_width;
    float screen_height;
    float z_offset;
    float z_scale;
};

struct RenderVertUniformBlockWithMapping : public RenderVertUniformBlock {
    uint64_t buffer_addresses[SCE_GXM_REAL_MAX_UNIFORM_BUFFER] = {};
};

// used internally to identify the field by the shader recompiler
// it is put next to the RenderVertUniformBlock so we don't forget to update both fields every time
enum VertUniformFieldId : uint32_t {
    VERT_UNIFORM_viewport_flip,
    VERT_UNIFORM_viewport_flag,
    VERT_UNIFORM_screen_width,
    VERT_UNIFORM_screen_height,
    VERT_UNIFORM_z_offset,
    VERT_UNIFORM_z_scale,
    VERT_UNIFORM_buffer_addresses
};

struct RenderFragUniformBlock {
    float back_disabled = 0;
    float front_disabled = 0;
    float writing_mask = 0;
    float use_raw_image = 0;
    int32_t res_multiplier = 0;
};

struct RenderFragUniformBlockWithMapping : public RenderFragUniformBlock {
    uint64_t buffer_addresses[SCE_GXM_REAL_MAX_UNIFORM_BUFFER] = {};
};

enum FragUniformFieldId : uint32_t {
    FRAG_UNIFORM_back_disabled,
    FRAG_UNIFORM_front_disabled,
    FRAG_UNIFORM_writing_mask,
    FRAG_UNIFORM_use_raw_image,
    FRAG_UNIFORM_res_multiplier,
    FRAG_UNIFORM_buffer_addresses
};

enum struct Target {
    GLSLOpenGL,
    SpirVOpenGL,
    SpirVVulkan
};

// Hints given while compiling the shader
// For example, the gxp shader knows at runtime the swizzle of the color surface
// or the format / number of components of the sampled images, but we can't
struct Hints {
    // used when symbols are stripped
    const std::vector<SceGxmVertexAttribute> *attributes;
    // color format of the surface, used for the opengl renderer when the GPU does not support features.support_unknown_format
    // I'm asking for the format instead of the base format as the swizzle might be used in future updates for surfaces with only 1 or 2 components
    SceGxmColorFormat color_format;

    // we need to have the texture formats of the sampled textures in two cases:
    // - when doing dependent texture queries with unknown format we need to know the format
    // - when sampling, we need to know the component count of a texture
    SceGxmTextureFormat vertex_textures[SCE_GXM_MAX_TEXTURE_UNITS];
    SceGxmTextureFormat fragment_textures[SCE_GXM_MAX_TEXTURE_UNITS];
};

struct GeneratedShader {
    std::string glsl;
    usse::SpirvCode spirv;
};

// Dump generated SPIR-V disassembly up to this point
void spirv_disasm_print(const usse::SpirvCode &spirv_binary, std::string *spirv_dump = nullptr);

// the returned object will only have its glsl or spirv field non-empty depending on the target
GeneratedShader convert_gxp(const SceGxmProgram &program, const std::string &shader_hash, const FeatureState &features, const Target target, const Hints &hints, bool maskupdate = false,
    bool force_shader_debug = false, std::function<bool(const std::string &ext, const std::string &dump)> dumper = nullptr);

void convert_gxp_to_glsl_from_filepath(const std::string &shader_filepath);

} // namespace shader
