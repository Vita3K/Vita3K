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

#include "state.h"

struct Config;
struct MemState;

namespace renderer::vulkan {

bool create(SDL_Window *window, std::unique_ptr<renderer::State> &state, const char *base_path);

bool create(VKState &state, std::unique_ptr<Context> &context, MemState &mem);
bool create(VKState &state, std::unique_ptr<RenderTarget> &rt, const SceGxmRenderTargetParams &params, const FeatureState &features);
void destroy(VKState &state, std::unique_ptr<RenderTarget> &rt);
bool create(std::unique_ptr<VertexProgram> &vp, VKState &state, const SceGxmProgram &program);
bool create(std::unique_ptr<FragmentProgram> &fp, VKState &state, const SceGxmProgram &program, const SceGxmBlendInfo *blend);

void draw(VKContext &context, SceGxmPrimitiveType type, SceGxmIndexFormat format,
    void *indices, size_t count, uint32_t instance_count, MemState &mem, const Config &config);

void new_frame(VKContext &context);

void set_context(VKContext &context, const MemState &mem, VKRenderTarget *rt, const FeatureState &features);
void set_uniform_buffer(VKContext &context, const ShaderProgram *program, const bool vertex_shader, const int block_num, const int size, const uint8_t *data);

void sync_clipping(VKContext &context);
void sync_stencil_func(VKContext &context, const bool is_back);
void sync_mask(VKContext &context, const MemState &mem);
void sync_depth_bias(VKContext &context);
void sync_depth_data(VKContext &context);
void sync_stencil_data(VKContext &context, const MemState &mem);
void sync_point_line_width(VKContext &context, const bool is_front);
void sync_texture(VKContext &context, MemState &mem, std::size_t index, SceGxmTexture texture, const Config &config,
    const std::string &base_path, const std::string &title_id);
void sync_viewport_flat(VKContext &context);
void sync_viewport_real(VKContext &context, const float xOffset, const float yOffset, const float zOffset,
    const float xScale, const float yScale, const float zScale);

void refresh_pipeline(VKContext &context);

namespace texture {

bool init(VKTextureCacheState &cache, const bool hashless_texture_cache);

void configure_bound_texture(VKTextureCacheState &cache, const SceGxmTexture &gxm_texture);
vk::Sampler create_sampler(VKState &state, const SceGxmTexture &gxm_texture, const uint16_t mip_count = 1);
void upload_bound_texture(VKTextureCacheState &cache, SceGxmTextureBaseFormat base_format, uint32_t width, uint32_t height,
    uint32_t mip_index, const void *pixels, int face, bool is_compressed, size_t pixels_per_stride);
void upload_done(VKTextureCacheState &cache);

} // namespace texture

} // namespace renderer::vulkan
