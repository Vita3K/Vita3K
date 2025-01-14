// Vita3K emulator project
// Copyright (C) 2025 Vita3K team
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

#include <array>
#include <condition_variable>
#include <memory>
#include <mutex>

namespace renderer {
struct FragmentProgram;
struct VertexProgram;
} // namespace renderer

struct SceGxmColorSurface {
    // note: the size is correct but the content does not match the PS Vita SceGxmColorSurface
    // opaque start
    struct {
        uint32_t disabled : 1;
        uint32_t downscale : 1;
        uint32_t gamma : 2;
        uint32_t : 28;
    };
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

struct SceGxmDepthStencilSurface {
    struct {
        uint32_t unk1 : 1; // always set to 1 (except disabled)
        uint32_t force_load : 1;
        uint32_t force_store : 1;
        uint32_t _stride : 8;
        uint32_t : 1;
        uint32_t _type_and_format : 20; // bits 0 and 4: type, bit 8: always set to 1 (except disabled), other bits are format
    };
    Ptr<void> depth_data;
    Ptr<void> stencil_data;
    float background_depth;
    struct {
        uint32_t stencil : 8;
        uint32_t mask : 1;
        uint32_t unk2 : 1; // always set to 1
        uint32_t : 22;
    };

    uint32_t get_stride() const {
        return (_stride + 1) << 5;
    }

    void set_stride(uint32_t stride) {
        _stride = (stride >> 5) - 1;
    }

    SceGxmDepthStencilSurfaceType get_type() const {
        return static_cast<SceGxmDepthStencilSurfaceType>((_type_and_format & 0x11) << 12);
    }

    void set_type(SceGxmDepthStencilSurfaceType type) {
        _type_and_format &= ~0x11;
        // also set the bit 8
        _type_and_format |= ((static_cast<uint32_t>(type) >> 12) & 0x11) | 0x100;
    }

    SceGxmDepthStencilFormat get_format() const {
        return static_cast<SceGxmDepthStencilFormat>((_type_and_format & 0x7EEE) << 12);
    }

    void set_format(SceGxmDepthStencilFormat format) {
        _type_and_format &= ~0x7EEE;
        _type_and_format |= (static_cast<uint32_t>(format) >> 12) & 0x7EEE;
    }

    bool disabled() const {
        return (_type_and_format & 0x7EEE) == 0;
    }
};

static_assert(sizeof(SceGxmDepthStencilSurface) == 5 * sizeof(uint32_t));

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

typedef Ptr<const void> UniformBuffer;
typedef std::array<UniformBuffer, SCE_GXM_REAL_MAX_UNIFORM_BUFFER> UniformBuffers;
typedef SceGxmTexture TextureData;
typedef Ptr<const void> StreamData;

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
    // timestamp_current is the timestamp for what has already been processed by the renderer
    std::atomic<uint32_t> timestamp_current;
    // timestamp_ahead is the timestamp for everything that has been asked to SceGxm so far (timestamp_ahead >= timestamp_current)
    std::atomic<uint32_t> timestamp_ahead;

    // timestamp for the last time the object was displayed
    std::atomic<uint32_t> last_display;

    // last signal operation done, given using the global timestamp
    uint32_t last_operation_global = 0;

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
    std::array<StreamData, SCE_GXM_MAX_VERTEX_STREAMS> stream_data;

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
    std::array<TextureData, SCE_GXM_MAX_TEXTURE_UNITS * 2> textures;

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

    // Visibility buffer
    bool visibility_enable = false;
    uint32_t visibility_index;
    bool visibility_is_increment = false;

    bool active = false;
};

struct SceGxmFragmentProgram {
    std::atomic<uint32_t> reference_count = 1;
    Ptr<const SceGxmProgram> program;
    // only necessary with async compilation
    std::atomic<uint32_t> compile_threads_on = 0;
    bool is_maskupdate;
    std::unique_ptr<renderer::FragmentProgram> renderer_data;
};

struct SceGxmNotification {
    Ptr<uint32_t> address;
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
    std::atomic<uint32_t> reference_count = 1;
    Ptr<const SceGxmProgram> program;
    std::vector<SceGxmVertexStream> streams;
    std::vector<SceGxmVertexAttribute> attributes;
    std::unique_ptr<renderer::VertexProgram> renderer_data;
    uint64_t key_hash;
    // only necessary with async compilation
    std::atomic<uint32_t> compile_threads_on = 0;
};

struct SceGxmPrecomputedDraw {
    Ptr<const SceGxmVertexProgram> program;

    SceGxmPrimitiveType type;

    Ptr<StreamData> stream_data;
    uint16_t stream_count;

    SceGxmIndexFormat index_format;
    Ptr<const void> index_data;
    uint32_t vertex_count;
    uint32_t instance_count;
};

struct SceGxmPrecomputedFragmentState {
    Ptr<const SceGxmFragmentProgram> program;

    Ptr<TextureData> textures;
    uint16_t texture_count;

    uint16_t buffer_count;
    Ptr<UniformBuffer> uniform_buffers;
};

struct SceGxmPrecomputedVertexState {
    Ptr<const SceGxmVertexProgram> program;

    Ptr<TextureData> textures;
    uint16_t texture_count;

    uint16_t buffer_count;
    Ptr<UniformBuffer> uniform_buffers;
};

static_assert(SCE_GXM_PRECOMPUTED_DRAW_WORD_COUNT * sizeof(uint32_t) >= sizeof(SceGxmPrecomputedDraw), "Precomputed Draw Size Too Big");
static_assert(SCE_GXM_PRECOMPUTED_VERTEX_STATE_WORD_COUNT * sizeof(uint32_t) >= sizeof(SceGxmPrecomputedVertexState), "Precomputed Vertex State Size Too Big");
static_assert(SCE_GXM_PRECOMPUTED_FRAGMENT_STATE_WORD_COUNT * sizeof(uint32_t) >= sizeof(SceGxmPrecomputedFragmentState), "Precomputed Fragment State Size Too Big");

struct SceGxmTransferImage {
    SceGxmTransferFormat format;
    Ptr<void> address;
    uint32_t x;
    uint32_t y;
    uint32_t width;
    uint32_t height;
    int32_t stride;
};
