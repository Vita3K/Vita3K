// Vita3K emulator project
// Copyright (C) 2021 Vita3K team
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

#include <crypto/hash.h>
#include <glutil/object.h>
#include <glutil/object_array.h>
#include <gxm/types.h>
#include <renderer/commands.h>
#include <renderer/gxm_types.h>

#include <array>
#include <map>
#include <string>
#include <tuple>
#include <vector>

static constexpr auto DEFAULT_RES_WIDTH = 960;
static constexpr auto DEFAULT_RES_HEIGHT = 544;

struct SceGxmProgram;
struct SDL_Window;

using UniformBufferSizes = std::array<std::uint32_t, 15>;

namespace renderer {

typedef std::map<GLuint, std::string> AttributeLocations;
typedef std::map<std::string, SharedGLObject> ShaderCache;
typedef std::tuple<std::string, std::string> ProgramHashes;
typedef std::map<ProgramHashes, SharedGLObject> ProgramCache;
typedef std::vector<std::string> ExcludedUniforms; // vector instead of unordered_set since it's much faster for few elements
typedef std::map<GLuint, GLenum> UniformTypes;

// State types
typedef std::map<Sha256Hash, const SceGxmProgram *> GXPPtrMap;

struct CommandBuffer;

enum class Backend {
    OpenGL,
#ifdef USE_VULKAN
    Vulkan,
#endif
};

enum class GXMState : std::uint16_t {
    RegionClip = 0,
    Program = 1,
    Viewport = 2,
    DepthBias = 3,
    DepthFunc = 4,
    DepthWriteEnable = 5,
    PolygonMode = 6,
    PointLineWidth = 7,
    StencilFunc = 8,
    Texture = 9,
    StencilRef = 10,
    VertexStream = 11,
    TwoSided = 12,
    CullMode = 13,
    Uniform = 14,
    UniformBuffer = 15,
    FragmentProgramEnable = 16,
    TotalState
};

enum SyncObjectSubject : std::uint32_t {
    None = 0,
    Fragment = 1 << 0,
    DisplayQueue = 1 << 1
};

struct RenderTarget;

struct GXMStreamInfo {
    std::uint8_t *data = nullptr;
    std::size_t size = 0;
};

struct GxmRecordState {
    // Programs.
    Ptr<const SceGxmFragmentProgram> fragment_program;
    Ptr<const SceGxmVertexProgram> vertex_program;

    std::array<GXMStreamInfo, SCE_GXM_MAX_VERTEX_STREAMS> vertex_streams;

    SceGxmColorSurface color_surface;
    SceGxmDepthStencilSurface depth_stencil_surface;

    SceGxmCullMode cull_mode = SCE_GXM_CULL_NONE;
    SceGxmTwoSidedMode two_sided = SCE_GXM_TWO_SIDED_DISABLED;
    SceGxmRegionClipMode region_clip_mode = SCE_GXM_REGION_CLIP_OUTSIDE;

    SceIVector2 region_clip_min;
    SceIVector2 region_clip_max;

    GxmStencilState front_stencil_state;
    GxmStencilState back_stencil_state;

    SceGxmDepthFunc front_depth_func = SCE_GXM_DEPTH_FUNC_LESS_EQUAL;
    SceGxmDepthFunc back_depth_func = SCE_GXM_DEPTH_FUNC_LESS_EQUAL;

    SceGxmDepthWriteMode front_depth_write_mode = SCE_GXM_DEPTH_WRITE_ENABLED;
    SceGxmDepthWriteMode back_depth_write_mode = SCE_GXM_DEPTH_WRITE_ENABLED;

    SceGxmFragmentProgramMode front_side_fragment_program_mode = SCE_GXM_FRAGMENT_PROGRAM_ENABLED;
    SceGxmFragmentProgramMode back_side_fragment_program_mode = SCE_GXM_FRAGMENT_PROGRAM_ENABLED;

    float writing_mask = 0.0f;
    bool viewport_flat = false;
    bool is_maskupdate = false;
};

struct Context {
    const RenderTarget *current_render_target{};
    GxmRecordState record;

    CommandList command_list;
    CommandAllocFunc alloc_func;
    CommandFreeFunc free_func;

    int render_finish_status = 0;
    int notification_finish_status = 0;

    std::string last_draw_fragment_program_hash;
    std::string last_draw_vertex_program_hash;

    virtual ~Context() = default;
};

struct ShaderProgram {
    std::string hash;
    UniformBufferSizes uniform_buffer_sizes; // Size of the buffer in 4-bytes unit
    UniformBufferSizes uniform_buffer_data_offsets; // Offset of the buffer in 4-bytes unit

    std::size_t max_total_uniform_buffer_storage;
};

struct FragmentProgram : ShaderProgram {
};

struct VertexProgram : ShaderProgram {
};

struct ShadersHash {
    std::string frag;
    std::string vert;
};

struct RenderTarget {
    int holder;
    virtual ~RenderTarget() = default;
};

} // namespace renderer
