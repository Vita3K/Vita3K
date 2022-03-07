// Vita3K emulator project
// Copyright (C) 2022 Vita3K team
//
// This file contains internal types used by Vita3K, and the
// internal Vita3K's implementation of SceGxm's opaque types.
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

#include <mem/ptr.h>
#include <util/types.h>

#include <gxm/types.h>

#include <condition_variable>
#include <memory>
#include <mutex>

namespace renderer {
struct FragmentProgram;
struct VertexProgram;
} // namespace renderer

struct SceGxmColorSurface {
    // opaque start
    uint32_t disabled : 1;
    uint32_t downscale : 1;
    uint32_t pad : 30;
    uint32_t width;
    uint32_t height;
    uint32_t strideInPixels;
    Ptr<void> data;
    SceGxmColorFormat colorFormat;
    SceGxmColorSurfaceType surfaceType;
    // opaque end
    uint32_t outputRegisterSize;
    SceGxmTexture backgroundTex;
};

static_assert(sizeof(SceGxmColorSurface) == (32 + sizeof(SceGxmTexture)), "Incorrect size.");

struct SceGxmDepthStencilControl {
    bool disabled;
    SceGxmDepthStencilFormat format;
    bool backgroundMask = true;
    uint8_t backgroundStencil;
};

struct SceGxmDepthStencilSurface {
    uint32_t zlsControl;
    Ptr<void> depthData;
    Ptr<void> stencilData;
    float backgroundDepth = 1.0f;
    Ptr<SceGxmDepthStencilControl> control;
};

struct SceGxmContextParams {
    Ptr<void> hostMem;
    uint32_t hostMemSize;
    Ptr<void> vdmRingBufferMem;
    uint32_t vdmRingBufferMemSize;
    Ptr<void> vertexRingBufferMem;
    uint32_t vertexRingBufferMemSize;
    Ptr<void> fragmentRingBufferMem;
    uint32_t fragmentRingBufferMemSize;
    Ptr<void> fragmentUsseRingBufferMem;
    uint32_t fragmentUsseRingBufferMemSize;
    uint32_t fragmentUsseRingBufferOffset;
};

struct SceGxmDeferredContextParams {
    Ptr<void> hostMem;
    uint32_t hostMemSize;
    Ptr<SceGxmDeferredContextCallback> vdmCallback;
    Ptr<SceGxmDeferredContextCallback> vertexCallback;
    Ptr<SceGxmDeferredContextCallback> fragmentCallback;
    Ptr<void> userData;
    Ptr<void> vdmBufferMem;
    uint32_t vdmBufferMemSize;
    Ptr<void> vertexBufferMem;
    uint32_t vertexBufferMemSize;
    Ptr<void> fragmentBufferMem;
    uint32_t fragmentBufferMemSize;
};

typedef std::array<Ptr<const void>, 15> UniformBuffers;
typedef std::array<SceGxmTexture, SCE_GXM_MAX_TEXTURE_UNITS * 2> TextureDatas;
typedef std::array<Ptr<const void>, SCE_GXM_MAX_VERTEX_STREAMS> StreamDatas;

struct GxmViewport {
    SceGxmViewportMode enable = SCE_GXM_VIEWPORT_ENABLED;
    SceFVector3 offset;
    SceFVector3 scale;
};

struct GxmStencilState {
    SceGxmStencilFunc func = SCE_GXM_STENCIL_FUNC_ALWAYS;
    SceGxmStencilOp stencil_fail = SCE_GXM_STENCIL_OP_KEEP;
    SceGxmStencilOp depth_fail = SCE_GXM_STENCIL_OP_KEEP;
    SceGxmStencilOp depth_pass = SCE_GXM_STENCIL_OP_KEEP;
    uint8_t compare_mask = 0xff; // TODO What's the default value?
    uint8_t write_mask = 0xff; // TODO What's the default value?
    uint8_t ref = 0; // TODO What's the default value?
};

enum class SceGxmLastReserveStatus {
    Available = 0, // No reservations have happened since last allocation
    Reserved = 1, // A reservation succeeded and space needs to be allocated
    AvailableAgain = 2 // TODO: Not sure what's the point of this third state (why libgxm doesn't set this back to Available)
};

struct SceGxmSyncObject {
    std::uint32_t done;

    std::mutex lock;
    std::condition_variable cond;
};

struct GxmContextState {
    // Constant after initialisation.
    SceGxmContextType type;

    // Surfaces.
    SceGxmColorSurface color_surface;
    SceGxmDepthStencilSurface depth_stencil_surface;

    // Clipping.
    SceGxmRegionClipMode region_clip_mode = SCE_GXM_REGION_CLIP_NONE;
    SceIVector2 region_clip_min;
    SceIVector2 region_clip_max;
    GxmViewport viewport;

    // Triangle setup?
    SceGxmCullMode cull_mode = SCE_GXM_CULL_NONE;
    SceGxmTwoSidedMode two_sided = SCE_GXM_TWO_SIDED_DISABLED;

    // Programs.
    Ptr<const SceGxmFragmentProgram> fragment_program;
    Ptr<const SceGxmVertexProgram> vertex_program;

    // Uniforms.
    UniformBuffers fragment_uniform_buffers;
    UniformBuffers vertex_uniform_buffers;
    size_t fragment_ring_buffer_used = 0;
    size_t vertex_ring_buffer_used = 0;
    SceGxmLastReserveStatus fragment_last_reserve_status = SceGxmLastReserveStatus::Available;
    SceGxmLastReserveStatus vertex_last_reserve_status = SceGxmLastReserveStatus::Available;

    Ptr<void> vertex_ring_buffer;
    uint32_t vertex_ring_buffer_size;
    Ptr<void> fragment_ring_buffer;
    uint32_t fragment_ring_buffer_size;
    Ptr<void> vdm_buffer;
    uint32_t vdm_buffer_size;

    // Vertex streams.
    StreamDatas stream_data;

    // Depth.
    SceGxmDepthFunc front_depth_func = SCE_GXM_DEPTH_FUNC_LESS_EQUAL;
    SceGxmDepthFunc back_depth_func = SCE_GXM_DEPTH_FUNC_LESS_EQUAL;
    SceGxmDepthWriteMode front_depth_write_enable = SCE_GXM_DEPTH_WRITE_ENABLED;
    SceGxmDepthWriteMode back_depth_write_enable = SCE_GXM_DEPTH_WRITE_ENABLED;

    // Stencil.
    GxmStencilState front_stencil;
    GxmStencilState back_stencil;

    // Polygon Mode
    SceGxmPolygonMode front_polygon_mode = SCE_GXM_POLYGON_MODE_TRIANGLE_FILL;
    SceGxmPolygonMode back_polygon_mode = SCE_GXM_POLYGON_MODE_TRIANGLE_FILL;

    // Fragment Program Mode
    SceGxmFragmentProgramMode front_side_fragment_program_mode = SCE_GXM_FRAGMENT_PROGRAM_ENABLED;
    SceGxmFragmentProgramMode back_side_fragment_program_mode = SCE_GXM_FRAGMENT_PROGRAM_ENABLED;

    // Line Width
    unsigned int front_point_line_width = 1;
    unsigned int back_point_line_width = 1;

    // Depth Bias
    int front_depth_bias_factor = 0;
    int front_depth_bias_units = 0;
    int back_depth_bias_factor = 0;
    int back_depth_bias_units = 0;

    // Textures.
    TextureDatas textures;

    // Mask
    bool writing_mask;

    // Fragment Sync Object
    Ptr<SceGxmSyncObject> fragment_sync_object;

    // Precomputed
    Ptr<SceGxmPrecomputedVertexState> precomputed_vertex_state;
    Ptr<SceGxmPrecomputedFragmentState> precomputed_fragment_state;

    // Deferred
    Ptr<SceGxmDeferredContextCallback> vertex_memory_callback;
    Ptr<SceGxmDeferredContextCallback> fragment_memory_callback;
    Ptr<SceGxmDeferredContextCallback> vdm_memory_callback;
    Ptr<void> memory_callback_userdata;

    bool active = false;
};

struct SceGxmFragmentProgram {
    size_t reference_count = 1;
    Ptr<const SceGxmProgram> program;
    bool is_maskupdate;
    std::unique_ptr<renderer::FragmentProgram> renderer_data;
};

struct SceGxmNotification {
    Ptr<volatile uint32_t> address;
    uint32_t value;
};

struct SceGxmRegisteredProgram {
    // TODO This is an opaque type.
    Ptr<const SceGxmProgram> program;
};

typedef Ptr<SceGxmRegisteredProgram> SceGxmShaderPatcherId;

typedef Ptr<void> SceGxmShaderPatcherHostAllocCallback(Ptr<void> userData, uint32_t size);
typedef void SceGxmShaderPatcherHostFreeCallback(Ptr<void> userData, Ptr<void> mem);
typedef Ptr<void> SceGxmShaderPatcherBufferAllocCallback(Ptr<void> userData, uint32_t size);
typedef void SceGxmShaderPatcherBufferFreeCallback(Ptr<void> userData, Ptr<void> mem);
typedef Ptr<void> SceGxmShaderPatcherUsseAllocCallback(Ptr<void> userData, uint32_t size, Ptr<uint32_t> usseOffset);
typedef void SceGxmShaderPatcherUsseFreeCallback(Ptr<void> userData, Ptr<void> mem);

struct SceGxmShaderPatcherParams {
    Ptr<void> userData;
    Ptr<SceGxmShaderPatcherHostAllocCallback> hostAllocCallback;
    Ptr<SceGxmShaderPatcherHostFreeCallback> hostFreeCallback;
    Ptr<SceGxmShaderPatcherBufferAllocCallback> bufferAllocCallback;
    Ptr<SceGxmShaderPatcherBufferFreeCallback> bufferFreeCallback;
    Ptr<void> bufferMem;
    uint32_t bufferMemSize;
    Ptr<SceGxmShaderPatcherUsseAllocCallback> vertexUsseAllocCallback;
    Ptr<SceGxmShaderPatcherUsseFreeCallback> vertexUsseFreeCallback;
    Ptr<void> vertexUsseMem;
    uint32_t vertexUsseMemSize;
    uint32_t vertexUsseOffset;
    Ptr<SceGxmShaderPatcherUsseAllocCallback> fragmentUsseAllocCallback;
    Ptr<SceGxmShaderPatcherUsseFreeCallback> fragmentUsseFreeCallback;
    Ptr<void> fragmentUsseMem;
    uint32_t fragmentUsseMemSize;
    uint32_t fragmentUsseOffset;
};

struct SceGxmVertexProgram {
    size_t reference_count = 1;
    Ptr<const SceGxmProgram> program;
    std::vector<SceGxmVertexStream> streams;
    std::vector<SceGxmVertexAttribute> attributes;
    std::unique_ptr<renderer::VertexProgram> renderer_data;
};

static constexpr size_t SCE_GXM_PRECOMPUTED_DRAW_EXTRA_SIZE = sizeof(StreamDatas);

struct SceGxmPrecomputedDraw {
    Ptr<const SceGxmVertexProgram> program;

    SceGxmPrimitiveType type;

    Ptr<StreamDatas> stream_data;
    uint16_t stream_count;

    SceGxmIndexFormat index_format;
    Ptr<const void> index_data;
    uint32_t vertex_count;
    uint32_t instance_count;
};

static constexpr size_t SCE_GXM_PRECOMPUTED_STATE_EXTRA_SIZE = sizeof(TextureDatas) + sizeof(UniformBuffers);

struct SceGxmPrecomputedFragmentState {
    Ptr<const SceGxmFragmentProgram> program;

    Ptr<TextureDatas> textures;
    uint16_t texture_count;

    Ptr<UniformBuffers> uniform_buffers;
};

struct SceGxmPrecomputedVertexState {
    Ptr<const SceGxmVertexProgram> program;

    Ptr<TextureDatas> textures;
    uint16_t texture_count;

    Ptr<UniformBuffers> uniform_buffers;
};

static_assert(SCE_GXM_PRECOMPUTED_DRAW_WORD_COUNT * sizeof(uint32_t) >= sizeof(SceGxmPrecomputedDraw), "Precomputed Draw Size Too Big");
static_assert(SCE_GXM_PRECOMPUTED_VERTEX_STATE_WORD_COUNT * sizeof(uint32_t) >= sizeof(SceGxmPrecomputedVertexState), "Precomputed Vertex State Size Too Big");
static_assert(SCE_GXM_PRECOMPUTED_FRAGMENT_STATE_WORD_COUNT * sizeof(uint32_t) >= sizeof(SceGxmPrecomputedFragmentState), "Precomputed Fragment State Size Too Big");
