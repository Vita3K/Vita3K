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

#include "metal_include.h"

#include <renderer/metal/state.h>
#include <renderer/metal/types.h>

#include <gxm/types.h>
#include <mem/ptr.h>
#include <renderer/texture_cache.h>
#include <renderer/types.h>
#include <shader/uniform_block.h>

#include "overlay_renderer.h"
#include "pipeline_cache.h"
#include "screen_renderer.h"
#include "surface_cache.h"

#include <array>
#include <map>
#include <mutex>
#include <vector>

struct MemState;

namespace renderer::metal {

constexpr int MAX_FRAMES_RENDERING = 3;
constexpr int NB_TEXTURE_STAGING_BUFFERS = 16;

constexpr bool is_frame_timestamp_in_flight(const uint64_t frame_timestamp, const uint64_t current_frame_timestamp) {
    return frame_timestamp != ~uint64_t{ 0 }
    && frame_timestamp <= current_frame_timestamp
        && current_frame_timestamp - frame_timestamp < MAX_FRAMES_RENDERING;
}

// CPU visible ring buffer, works the same way as vkutil::HostRingBuffer
struct RingBuffer {
    id<MTLBuffer> buffer = nil;
    uint32_t cursor = 0;
    uint32_t capacity = 0;
    uint32_t alignment = 256;
    uint32_t data_offset = 0;

    void create(id<MTLDevice> device, uint32_t capacity, NSString *label);
    // allocate new data, data_offset is then the offset of the allocation
    void allocate(uint32_t data_size);
    void copy(uint32_t size, const void *data, uint32_t offset = 0);
    void allocate(uint32_t data_size, const void *data) {
        allocate(data_size);
        copy(data_size, data);
    }
};

struct TextureStagingBuffer {
    id<MTLBuffer> buffer = nil;
    uint32_t size = 0;
    uint32_t used_so_far = 0;
    uint64_t scene_timestamp = ~0;
    uint64_t frame_timestamp = ~0;
    id<MTLCommandBuffer> waiting_cmd = nil;
};

struct TextureCacheEntry {
    id<MTLTexture> texture = nil;
    MTLPixelFormat format = MTLPixelFormatInvalid;
    uint32_t width = 0;
    uint32_t height = 0;
    bool is_cube = false;
    uint16_t mip_count = 1;
    uint32_t memory_needed = 0;
};

class MetalTextureCache : public TextureCache {
public:
    MetalState &state;

    TextureStagingBuffer staging_buffers[NB_TEXTURE_STAGING_BUFFERS];
    uint32_t staging_idx = 0;
    uint64_t last_waited_scene = 0;
    uint64_t current_scene_timestamp = 0;

    std::array<TextureCacheEntry, TextureCacheSize> textures;
    std::vector<id<MTLSamplerState>> samplers;

    TextureCacheEntry *current_texture = nullptr;
    bool is_texture_transfer_ready = false;

    explicit MetalTextureCache(MetalState &state);

    void prepare_staging_buffer(bool is_configure = false);

    bool init(const bool hashless_texture_cache, const fs::path &texture_folder, const std::string_view game_id);
    void select(size_t index, const SceGxmTexture &texture) override;
    void configure_texture(const SceGxmTexture &texture) override;
    void upload_texture_impl(SceGxmTextureBaseFormat base_format, uint32_t width, uint32_t height, uint32_t mip_index, const void *pixels, int face, uint32_t pixels_per_stride) override;
    void upload_done() override;

    void configure_sampler(size_t index, const SceGxmTexture &texture, bool no_linear) override;

    void import_configure_impl(SceGxmTextureBaseFormat base_format, uint32_t width, uint32_t height, bool is_srgb, uint16_t nb_components, uint16_t mipcount, bool swap_rb) override;

    id<MTLSamplerState> get_retrieved_sampler() const {
        return samplers[last_bound_sampler_index];
    }

    void cleanup();

private:
    void create_current_texture(uint32_t width, uint32_t height, MTLPixelFormat format, bool is_cube, uint16_t mip_count, const MTLTextureSwizzleChannels &swizzle);
};

struct FrameObject {
    // command buffers submitted during this frame
    std::vector<id<MTLCommandBuffer>> rendered_cmds;
    uint64_t frame_timestamp = 0;
};

struct MetalDevice {
    MetalState &state;

    id<MTLDevice> device = nil;
    id<MTLCommandQueue> queue = nil;

    bool is_apple_gpu = false;
    // can the fragment shader read the color attachment with [[color(0)]]
    bool support_framebuffer_fetch = false;
    // 16-bit packed formats (ABGR4, B5G6R5, A1BGR5...)
    bool support_packed16 = false;
    bool support_bc = false;
    bool support_astc = false;
    // read-write textures with the rgba8 format (needed for the mask)
    bool support_rw_texture = false;
    uint32_t max_texture_size = 8192;
    uint32_t max_anisotropy = 16;

    id<MTLTexture> default_texture = nil;
    id<MTLSamplerState> default_sampler = nil;
    id<MTLBuffer> default_buffer = nil;

    std::array<FrameObject, MAX_FRAMES_RENDERING> frames;
    uint64_t frame_timestamp = 1;
    int current_frame_idx = 1;

    MetalTextureCache texture_cache;
    SurfaceCache surface_cache;
    PipelineCache pipeline_cache;
    ScreenRenderer screen_renderer;
    OverlayRenderer overlay_renderer;

    explicit MetalDevice(MetalState &state);

    FrameObject &frame() {
        return frames[current_frame_idx];
    }

    // wait for every submitted command buffer to be done
    void wait_idle();
};

struct BoundTexture {
    id<MTLTexture> texture = nil;
    id<MTLSamplerState> sampler = nil;
};

struct VisibilityBuffer {
    Address address;
    // size in uint32_t units
    uint32_t size;
    std::map<uint32_t, uint64_t> result_scenes;
};

struct VisibilityQuery {
    VisibilityBuffer *buffer;
    uint32_t index;
    bool increment;
    uint32_t slot;
};

struct MetalContextImpl {
    MetalContext &owner;
    MetalState &state;
    MemState &mem;

    uint64_t scene_timestamp = 1;
    // Unlike scene_timestamp, recording splits do not advance the logical GXM scene.
    uint64_t gxm_scene_id = 0;

    RingBuffer vertex_stream_ring_buffer;
    RingBuffer index_stream_ring_buffer;
    RingBuffer vertex_uniform_stream_ring_buffer;
    RingBuffer fragment_uniform_stream_ring_buffer;
    RingBuffer vertex_info_uniform_buffer;
    RingBuffer fragment_info_uniform_buffer;

    BoundTexture vertex_textures[SCE_GXM_MAX_TEXTURE_UNITS];
    BoundTexture fragment_textures[SCE_GXM_MAX_TEXTURE_UNITS];

    bool vertex_uniform_storage_allocated = false;
    bool fragment_uniform_storage_allocated = false;

    id<MTLBuffer> vertex_stream_buffers[SCE_GXM_MAX_VERTEX_STREAMS] = {};
    uint32_t vertex_stream_offsets[SCE_GXM_MAX_VERTEX_STREAMS] = {};

    shader::RenderVertUniformBlock prev_vert_ublock;
    shader::RenderFragUniformBlock prev_frag_ublock;

    shader::RenderVertUniformBlockExtended curr_vert_ublock;
    shader::RenderFragUniformBlockExtended curr_frag_ublock;

    uint8_t shader_info_temp[std::max(shader::RenderVertUniformBlockExtended::get_max_size(), shader::RenderFragUniformBlockExtended::get_max_size())];

    // Visibility buffer emulation
    static constexpr uint32_t max_visibility_queries = 4096;
    std::map<Address, VisibilityBuffer> visibility_buffers;
    VisibilityBuffer *current_visibility_buffer = nullptr;
    std::vector<VisibilityQuery> visibility_queries;
    // one slot (8 bytes) per query, one buffer per scene
    id<MTLBuffer> visibility_result_buffer = nil;
    bool is_in_query = false;
    int current_query_idx = -1;
    bool is_query_op_increment = false;

    MetalRenderTarget *render_target = nullptr;
    MTLViewport viewport{};
    MTLScissorRect scissor{};
    SceGxmPrimitiveType last_primitive = SCE_GXM_PRIMITIVE_TRIANGLES;

    // color attachment used by the current scene
    id<MTLTexture> current_color_view = nil;
    id<MTLTexture> current_color_base = nil;
    id<MTLTexture> current_ds_texture = nil;
    MTLPixelFormat current_color_format = MTLPixelFormatRGBA8Unorm;
    bool color_is_transient = false;

    bool is_recording = false;
    bool in_renderpass = false;
    bool in_maskupdate_pass = false;
    bool refresh_pipeline = false;
    bool is_first_scene_draw = false;
    // was the depth-stencil cleared (not loaded) at the beginning of the scene
    bool initial_depth_stencil_clear = false;
    // does the next render pass need to clear the depth-stencil (instead of loading it)
    bool depth_stencil_clear_pending = false;
    float ds_clear_depth = 1.0f;
    uint32_t ds_clear_stencil = 0;
    // does the next render pass need to clear the color attachment (freshly created surface)
    bool color_clear_pending = false;
    // first render pass of the scene (the transient color does not need to be loaded)
    bool first_pass_of_scene = false;
    // size of the attachments of the current render pass
    uint32_t framebuffer_width = 0;
    uint32_t framebuffer_height = 0;
    uint16_t initialized_macroblocks = 0;

    id<MTLCommandBuffer> render_cmd = nil;
    // command buffer executed before render_cmd, used for texture uploads and copies
    id<MTLCommandBuffer> prerender_cmd = nil;
    id<MTLBlitCommandEncoder> prerender_blit = nil;
    id<MTLRenderCommandEncoder> encoder = nil;

    id<MTLRenderPipelineState> current_pipeline = nil;
    id<MTLDepthStencilState> current_depth_stencil = nil;
    // state bound to the current encoder
    bool raster_state_dirty = true;
    bool vertex_textures_dirty = true;
    bool fragment_textures_dirty = true;
    uint16_t bound_vertex_texture_count = 0;
    uint16_t bound_fragment_texture_count = 0;
    id<MTLBuffer> bound_vertex_buffers[SCE_GXM_MAX_VERTEX_STREAMS] = {};
    uint32_t bound_vertex_offsets[SCE_GXM_MAX_VERTEX_STREAMS] = {};

    // used for macroblock sync emulation
    uint16_t last_macroblock_x = ~0;
    uint16_t last_macroblock_y = ~0;
    bool ignore_macroblock = false;

    MetalContextImpl(MetalContext &owner, MetalState &state, MemState &mem);
    ~MetalContextImpl();

    // get (and create if needed) the blit encoder of the prerender command buffer
    id<MTLBlitCommandEncoder> blit();
    void end_prerender_blit();
    // submit the prerender command buffer now and start a new one (used when all staging buffers are busy)
    void flush_prerender();

    void start_recording(bool first_in_scene = false);
    void start_render_pass(bool maskupdate = false);
    void stop_render_pass();
    void stop_recording(bool submit = true);

    void begin_visibility_query();
    void end_visibility_query();
    void resolve_visibility_queries();

    // check (when the render target has macroblock set) if we are drawing to another block
    void check_for_macroblock_change(bool is_draw);

    // emulate vkCmdClearAttachments on the depth-stencil inside the current render pass
    void clear_depth_stencil_region(const MTLScissorRect &region, bool clear_depth, bool clear_stencil);
    // re-apply the dynamic state after a new encoder is created
    void apply_dynamic_state();
};

struct MetalRenderTargetImpl {
    id<MTLTexture> color = nil;
    id<MTLTexture> depthstencil = nil;
    id<MTLTexture> mask = nil;
    bool mask_may_reject = true;
    uint64_t last_used_frame = 0;
};

// helpers
id<MTLTexture> create_texture(id<MTLDevice> device, MTLPixelFormat format, uint32_t width, uint32_t height, MTLTextureUsage usage,
    uint32_t mip_count = 1, bool is_cube = false, uint32_t array_length = 1, MTLStorageMode storage = MTLStorageModePrivate, const char *label = nullptr);
id<MTLTexture> create_texture_view(id<MTLTexture> texture, MTLPixelFormat format, const MTLTextureSwizzleChannels &swizzle);
id<MTLBuffer> create_shared_buffer(id<MTLDevice> device, size_t size, const char *label = nullptr);

constexpr MTLPixelFormat depth_stencil_format = MTLPixelFormatDepth32Float_Stencil8;
constexpr MTLPixelFormat stencil_view_format = MTLPixelFormatX32_Stencil8;

} // namespace renderer::metal
