// Vita3K emulator project
// Copyright (C) 2026 Vita3K team
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

#include <util/fs.h>

#include <cstdint>
#include <string>
#include <vector>

namespace renderer::metal::msl {

// Metal resource indices used by the translated gxm shaders

// vertex stage buffers: vertex streams use indices 0 to SCE_GXM_MAX_VERTEX_STREAMS - 1
constexpr uint32_t VERTEX_RENDER_INFO_BUFFER_INDEX = 16;
constexpr uint32_t VERTEX_GXM_UNIFORM_BUFFER_INDEX = 17;
// fragment stage buffers
constexpr uint32_t FRAGMENT_RENDER_INFO_BUFFER_INDEX = 0;
constexpr uint32_t FRAGMENT_GXM_UNIFORM_BUFFER_INDEX = 1;
// fragment stage textures (the 16 texture units use indices 0 to 15)
constexpr uint32_t MASK_TEXTURE_INDEX = 16;
constexpr uint32_t COLOR_ATTACHMENT_RAW_TEXTURE_INDEX = 17;
constexpr uint32_t COLOR_ATTACHMENT_TEXTURE_INDEX = 18;
// used by SPIRV-Cross for buffer sizes if a shader ever needs them, never bound
constexpr uint32_t BUFFER_SIZE_BUFFER_INDEX = 30;
constexpr uint32_t SWIZZLE_BUFFER_INDEX = 29;

// builtin (screen / overlay) pipelines
constexpr uint32_t BUILTIN_VERTEX_BUFFER_INDEX = 0;
constexpr uint32_t BUILTIN_PUSH_CONSTANT_BUFFER_INDEX = 1;

enum class Stage {
    Vertex,
    Fragment,
    Compute
};

struct BindingRemap {
    uint32_t set;
    uint32_t binding;
    uint32_t msl_buffer;
    uint32_t msl_texture;
    uint32_t msl_sampler;
};

struct TranslateResult {
    bool ok = false;
    std::string source;
    std::string entry_point;
    std::string error;
};

// Translate a gxm shader (SPIR-V generated for the Vulkan target) to MSL
TranslateResult translate_gxm_shader(const std::vector<uint32_t> &spirv, bool is_vertex, bool use_framebuffer_fetch);

// Translate a builtin shader, push constants are mapped to push_constant_buffer_index
TranslateResult translate_builtin_shader(const std::vector<uint32_t> &spirv, Stage stage, const std::vector<BindingRemap> &remaps, uint32_t push_constant_buffer_index);

std::vector<uint32_t> load_spirv_file(const fs::path &path);

} // namespace renderer::metal::msl
