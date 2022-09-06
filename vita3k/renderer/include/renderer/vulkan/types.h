// Vita3K emulator project
// Copyright (C) 2025 Vita3K team
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

#include <renderer/texture_cache.h>
#include <renderer/types.h>
#include <shader/uniform_block.h>
#include <vkutil/objects.h>

struct MemState;

namespace renderer::vulkan {

struct VKState;
struct VKRenderTarget;

constexpr int MAX_FRAMES_RENDERING = 3;
constexpr int NB_TEXTURE_STAGING_BUFFERS = 16;

struct TextureStagingBuffer {
    vkutil::Buffer buffer;
    uint32_t used_so_far;
    uint64_t scene_timestamp = ~0;
    uint64_t frame_timestamp = ~0;
    vk::Fence waiting_fence;
};

struct TextureCacheEntry {
    vkutil::Image texture;
    bool is_cube;
    uint16_t mip_count;
    uint32_t memory_needed;
};

struct VKTextureCache : public TextureCache {
    VKState &state;

    TextureStagingBuffer staging_buffers[NB_TEXTURE_STAGING_BUFFERS];
    uint32_t staging_idx = 0;
    uint64_t last_waited_scene = 0;
    uint64_t current_scene_timestamp;

    std::array<TextureCacheEntry, TextureCacheSize> textures;
    std::vector<vk::Sampler> samplers;

    TextureCacheEntry *current_texture = nullptr;
    const SceGxmTexture *gxm_texture = nullptr;
    vk::CommandBuffer cmd_buffer = nullptr;
    bool is_texture_transfer_ready = false;

    VKTextureCache(VKState &state);
    // get an available staging buffer, wait for one if all are busy
    void prepare_staging_buffer(bool is_configure = false);

    bool init(const bool hashless_texture_cache, const fs::path &texture_folder, const std::string_view game_id);
    void select(size_t index, const SceGxmTexture &texture) override;
    void configure_texture(const SceGxmTexture &texture) override;
    void upload_texture_impl(SceGxmTextureBaseFormat base_format, uint32_t width, uint32_t height, uint32_t mip_index, const void *pixels, int face, uint32_t pixels_per_stride) override;
    void upload_done() override;

    void configure_sampler(size_t index, const SceGxmTexture &texture, bool no_linear) override;

    void import_configure_impl(SceGxmTextureBaseFormat base_format, uint32_t width, uint32_t height, bool is_srgb, uint16_t nb_components, uint16_t mipcount, bool swap_rb) override;

    vk::Sampler get_retrieved_sampler() const {
        return samplers[last_bound_sampler_index];
    }
};

struct FrameDescriptor {
    std::vector<vk::DescriptorSet> sets;
    int descriptors_idx = 0;
};

struct FrameObject {
    vk::CommandPool render_pool;
    // we need to have a specific prerender pool because prerender command buffer
    // can be reset if we use too many new textures at once
    vk::CommandPool prerender_pool;

    std::vector<vk::Fence> rendered_fences;
    // equals to context.frame_timestamp when the frame object is used
    uint64_t frame_timestamp;

    // there are at most 16 different textures for a given stage
    // stage_descriptor[i] is the descriptor when using (i+1) textures
    FrameDescriptor vert_descriptors[16];
    FrameDescriptor frag_descriptors[16];

    // descriptor for the color surface
    FrameDescriptor color_descriptor;

    // destroy gpu objects MAX_FRAMES_RENDERING frames later to make sure they are no longer being used
    vkutil::DestroyQueue destroy_queue;
};

struct MappedMemoryBuffer {
    vk::DeviceMemory memory;
    vk::Buffer buffer;
};

struct ExternalBuffer {
    vk::DeviceMemory memory;
    void *extra;
};

struct MappedMemory {
    Address address;
    std::variant<ExternalBuffer, vkutil::Buffer> buffer_impl;
    vk::Buffer buffer;
    uint32_t size;
    uint64_t buffer_address;
};

enum struct BufferType {
    Storage,
    Vertex,
    Index16,
    Index32
};

struct TrappedBuffer {
    uint32_t size;
    // used by the index buffer to keep the max index
    uint32_t extra;
    // no need for it to be atomic
    bool dirty = false;
    uint8_t *mapped_location;

    TrappedBuffer() {}
};

// structure to track which buffer were trapped and if they have been modified
// this is used for: storage buffers, index buffers and vertex buffers
struct BufferTrapping {
    std::map<Address, TrappedBuffer> trapped_buffers;
    // Used when no buffer trapping is applied
    TrappedBuffer temp_buffer;

    VKState &state;

    BufferTrapping(VKState &state);
    TrappedBuffer *access_buffer(Address addr, uint32_t size, MemState &mem, bool always_trap = false, bool cover_everything = false);
    void remove_range(Address start, Address end);
};

// Use vulkan queries to implement visibility buffer
struct VisibilityBuffer {
    Address address;
    vk::Buffer gpu_buffer;
    uint64_t buffer_offset;
    uint32_t size;
    vk::QueryPool query_pool;
    std::vector<bool> queries_used; // the queries that were used in the current scene
};

struct FenceWaitRequest {
    vk::Fence fence;
};

// request to trigger a notification after the previous fences have been waited for
struct NotificationRequest {
    SceGxmNotification notifications[2];
};

struct FrameDoneRequest {
    uint64_t frame_timestamp;
};

struct SyncSignalRequest {
    SceGxmSyncObject *sync;
    uint32_t timestamp;
};
struct ColorSurfaceCacheInfo;

struct PostSurfaceSyncRequest {
    ColorSurfaceCacheInfo *cache_info;
};

using CallbackRequestFunction = std::function<void()>;
struct CallbackRequest {
    // use a pointer so the size is similar to other elements of WaitThreadRequest
    // and not to have to mess with move semantics
    CallbackRequestFunction *callback;
};

// only used with the DoubleBuffer Method
// copy the range [location, location+size] from the GPU buffer to the CPU buffer
struct BufferSyncRequest {
    Address location;
    uint32_t size;
};

// A parallel thread is handling these request and telling other waiting threads
// when they are done
// only used if memory mapping is enabled
typedef std::variant<
    FenceWaitRequest,
    NotificationRequest,
    FrameDoneRequest,
    BufferSyncRequest,
    PostSurfaceSyncRequest,
    SyncSignalRequest,
    CallbackRequest>
    WaitThreadRequest;

struct VKContext : public renderer::Context {
    // GXM Context Info
    VKState &state;

    MemState &mem;

    uint64_t frame_timestamp = 1;
    uint64_t scene_timestamp = 1;
    std::vector<vk::CommandBuffer> cmdbuffers_to_submit = {};

    vkutil::HostRingBuffer vertex_stream_ring_buffer;
    vkutil::HostRingBuffer index_stream_ring_buffer;
    vkutil::HostRingBuffer vertex_uniform_stream_ring_buffer;
    vkutil::HostRingBuffer fragment_uniform_stream_ring_buffer;
    vkutil::HostRingBuffer vertex_info_uniform_buffer;
    vkutil::HostRingBuffer fragment_info_uniform_buffer;

    vk::DescriptorImageInfo vertex_textures[SCE_GXM_MAX_TEXTURE_UNITS] = {};
    vk::DescriptorImageInfo fragment_textures[SCE_GXM_MAX_TEXTURE_UNITS] = {};

    bool vertex_uniform_storage_allocated = false;
    bool fragment_uniform_storage_allocated = false;

    vk::Buffer vertex_stream_buffers[SCE_GXM_MAX_VERTEX_STREAMS];
    vk::DeviceSize vertex_stream_offsets[SCE_GXM_MAX_VERTEX_STREAMS] = {};

    shader::RenderVertUniformBlock prev_vert_ublock;
    shader::RenderFragUniformBlock prev_frag_ublock;

    shader::RenderVertUniformBlockExtended curr_vert_ublock;
    shader::RenderFragUniformBlockExtended curr_frag_ublock;

    // scratch memory to contain the constructed render info uniform before copying it
    uint8_t shader_info_temp[std::max(shader::RenderVertUniformBlockExtended::get_max_size(), shader::RenderFragUniformBlockExtended::get_max_size())];

    // used to implement the Visibility Buffer
    std::map<Address, VisibilityBuffer> visibility_buffers;
    VisibilityBuffer *current_visibility_buffer = nullptr;
    int visibility_max_used_idx = -1;
    bool is_in_query = false;
    int current_query_idx = -1;
    bool is_query_op_increment = false;

    // descriptor pool for dynamic uniforms (allocated once for the whole game)
    vk::DescriptorPool global_descriptor_pool;
    // we will use this descriptor set for all the draws
    vk::DescriptorSet global_set;
    // descriptor set when using 0 textures
    vk::DescriptorSet empty_set;

    // descriptor set used to store the mask and the color attachment
    vk::DescriptorSet rendertarget_set;

    uint16_t last_vert_texture_count = ~0;
    vk::DescriptorSet last_vert_texture_descriptor;
    uint16_t last_frag_texture_count = ~0;
    vk::DescriptorSet last_frag_texture_descriptor;

    VKRenderTarget *render_target = nullptr;
    vk::Viewport viewport;
    vk::Rect2D scissor;
    SceGxmPrimitiveType last_primitive;

    vk::RenderPass current_render_pass;
    vk::RenderPass current_shader_interlock_pass = nullptr;
    vk::Pipeline current_pipeline;

    vk::Framebuffer current_framebuffer;
    vk::Framebuffer current_shader_interlock_framebuffer = nullptr;
    // we need the format or image for some cases
    vkutil::Image *current_color_base_image;
    vk::Format current_color_format;
    vk::ImageView current_color_view;
    vk::ImageView current_ds_view;

    bool is_recording = false;
    bool in_renderpass = false;
    bool refresh_pipeline = false;
    bool is_first_scene_draw = false;
    // command buffer used to record the current scene
    vk::CommandBuffer render_cmd{};
    // command buffer used for commands that need to be executed before render_cmd (mostly because they can't be done during a render pass)
    vk::CommandBuffer prerender_cmd{};
    // next fence to be used to wait for the current scene
    vk::Fence next_fence{};
    VKRenderTarget *cmd_target = nullptr;

    // used for macroblock sync emulation
    uint16_t last_macroblock_x = ~0;
    uint16_t last_macroblock_y = ~0;
    // special case where we can't determine the current macroblock
    bool ignore_macroblock = false;

    // used if necessary to restart easily the render pass
    vk::RenderPassBeginInfo curr_renderpass_info;
    // only useful if shader interlock is enabled, to know if we need to transition
    bool last_draw_was_framebuffer_fetch;

    // only used if memory mapping is enabled
    std::mutex new_frame_mutex;
    std::condition_variable new_frame_condv;
    std::thread gpu_request_wait_thread;
    uint64_t last_frame_waited = 0;

    explicit VKContext(VKState &state, MemState &mem);
    // TODO: properly destroy the context
    ~VKContext() override;

    void start_recording(bool first_in_scene = false);
    void start_render_pass(bool create_descriptor_set = true);
    void stop_render_pass();
    void stop_recording(const SceGxmNotification &notif1, const SceGxmNotification &notif2, bool submit = true);

    // check (when the render target has macroblock set) if we are drawing to another block
    void check_for_macroblock_change(bool is_draw);

private:
    void wait_thread_function(const MemState &mem);
};

struct VKRenderTarget : public renderer::RenderTarget {
    uint16_t width;
    uint16_t height;
    vkutil::Image color;
    vkutil::Image depthstencil;

    uint64_t last_used_frame = 0;

    // sync objects
    std::vector<vk::Fence> fences;
    // the current fence we're at
    int fence_idx = 0;

    std::array<std::vector<vk::CommandBuffer>, MAX_FRAMES_RENDERING> cmd_buffers;
    // command buffers used for the pre-render, can be reset
    std::array<std::vector<vk::CommandBuffer>, MAX_FRAMES_RENDERING> pre_cmd_buffers;
    // the command buffer index we're at in a frame
    int cmd_buffer_idx = 0;

    VKRenderTarget(VKState &state, const SceGxmRenderTargetParams &params);
    ~VKRenderTarget() override = default;
};

struct VKFragmentProgram : public renderer::FragmentProgram {
    vk::PipelineColorBlendAttachmentState blending;
    uint64_t blending_hash;
};
} // namespace renderer::vulkan
