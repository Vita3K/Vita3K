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

#include <renderer/texture_cache_state.h>
#include <renderer/types.h>

#include <vkutil/objects.h>

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

struct VKTextureCacheState : public renderer::TextureCacheState {
    VKState &state;

    TextureStagingBuffer staging_buffers[NB_TEXTURE_STAGING_BUFFERS];
    uint32_t staging_idx = 0;
    uint64_t last_waited_scene = 0;

    std::array<TextureCacheEntry, TextureCacheSize> textures;

    TextureCacheEntry *current_texture = nullptr;
    const SceGxmTexture *gxm_texture = nullptr;
    vk::CommandBuffer cmd_buffer = nullptr;
    bool is_texture_transfer_ready = false;

    VKTextureCacheState(VKState &state);
    // get an available staging buffer, wait for one if all are busy
    void prepare_staging_buffer(bool is_configure = false);
};

struct FrameObject {
    vk::CommandPool render_pool;
    // we need to have a specif prerender pool because prerender command buffer
    // can be reset if we use too many new textures at once
    vk::CommandPool prerender_pool;
    vk::DescriptorPool descriptor_pool;

    std::vector<vk::Fence> rendered_fences;

    // destroy gpu objects MAX_FRAMES_RENDERING frames later to make sure they are no longer being used
    vkutil::DestroyQueue destroy_queue;
};

struct VKContext : public renderer::Context {
    // GXM Context Info
    VKState &state;

    std::array<FrameObject, MAX_FRAMES_RENDERING> frames;
    int current_frame_idx = 0;
    uint64_t frame_timestamp = 0;
    uint64_t scene_timestamp = 0;

    vkutil::HostRingBuffer vertex_stream_ring_buffer;
    vkutil::HostRingBuffer index_stream_ring_buffer;
    vkutil::HostRingBuffer vertex_uniform_stream_ring_buffer;
    vkutil::HostRingBuffer fragment_uniform_stream_ring_buffer;
    vkutil::LocalRingBuffer vertex_info_uniform_buffer;
    vkutil::LocalRingBuffer fragment_info_uniform_buffer;

    vk::DescriptorImageInfo vertex_textures[SCE_GXM_MAX_TEXTURE_UNITS] = {};
    vk::DescriptorImageInfo fragment_textures[SCE_GXM_MAX_TEXTURE_UNITS] = {};

    bool vertex_uniform_storage_allocated = false;
    bool fragment_uniform_storage_allocated = false;

    std::array<vk::DeviceSize, SCE_GXM_MAX_VERTEX_STREAMS> vertex_buffer_offsets = {};

    // descriptor pool for dynamic uniforms (allocated once for the whole game)
    vk::DescriptorPool global_descriptor_pool;
    // we will use this descriptor set for all the draws
    vk::DescriptorSet global_set;

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
    vk::Pipeline current_pipeline;

    vk::Framebuffer current_framebuffer;
    uint16_t current_framebuffer_height;
    vkutil::Image *current_color_attachment;
    vkutil::Image *current_ds_attachment;

    bool is_recording = false;
    bool in_renderpass = false;
    bool refresh_pipeline = false;
    // command buffer used to record the current scene
    vk::CommandBuffer render_cmd{};
    // command buffer used for commands that need to be executed before render_cmd (mostly because they can't be done during a render pass)
    vk::CommandBuffer prerender_cmd{};
    VKRenderTarget *cmd_target = nullptr;

    // image used when a shader read from an image that has not been set
    vkutil::Image default_image;

    inline FrameObject &frame() {
        return frames[current_frame_idx];
    }

    explicit VKContext(VKState &state);
    // TODO: properly destroy the context
    ~VKContext() override = default;

    void start_recording();
    void start_render_pass();
    void stop_render_pass();
    void stop_recording();
};

struct VKRenderTarget : public renderer::RenderTarget {
    uint16_t width;
    uint16_t height;
    vkutil::Image mask;
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

    VKRenderTarget(VKState &state, vma::Allocator allocator, uint16_t width, uint16_t height, uint16_t samples_per_frame);
    ~VKRenderTarget() override = default;
};

struct VKFragmentProgram : public renderer::FragmentProgram {
    vk::PipelineColorBlendAttachmentState blending;
    uint64_t blending_hash;
};

// used with SceGxmSyncObject
struct SyncExtraData {
    VKRenderTarget *render_target = nullptr;
    // fences the display queue must wait for before displaying the buffer
    std::vector<vk::Fence> fences;
};
} // namespace renderer::vulkan
