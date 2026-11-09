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

#include <renderer/metal/state.h>
#include <renderer/metal/types.h>

#include <gxm/types.h>
#include <mem/ptr.h>

#include <memory>
#include <string_view>

struct Config;
struct FeatureState;
struct MemState;

namespace renderer::metal {

bool create(std::unique_ptr<renderer::State> &state, const Config &config);

bool create(MetalState &state, std::unique_ptr<Context> &context, MemState &mem);
bool create(MetalState &state, std::unique_ptr<RenderTarget> &rt, const SceGxmRenderTargetParams &params, const FeatureState &features);
void destroy(MetalState &state, std::unique_ptr<RenderTarget> &rt);
bool create(std::unique_ptr<VertexProgram> &vp, MetalState &state, const SceGxmProgram &program);
bool create(std::unique_ptr<FragmentProgram> &fp, MetalState &state, const SceGxmProgram &program, const SceGxmBlendInfo *blend);

void draw(MetalContext &context, SceGxmPrimitiveType type, SceGxmIndexFormat format,
    Ptr<void> indices, size_t count, uint32_t instance_count, MemState &mem, const Config &config);

void new_frame(MetalContext &context);

void set_context(MetalContext &context, MemState &mem, MetalRenderTarget *rt, const FeatureState &features);
void set_uniform_buffer(MetalContext &context, MemState &mem, const ShaderProgram *program, const bool vertex_shader, const int block_num, const int size, Ptr<uint8_t> data);

// is the context currently recording a scene?
bool is_recording(const MetalContext &context);
// end the current scene and submit it to the GPU
void stop_recording(MetalContext &context);

void sync_clipping(MetalContext &context);
void sync_stencil_func(MetalContext &context, const bool is_back);
void sync_depth_bias(MetalContext &context);
void sync_point_line_width(MetalContext &context, const bool is_front);
void sync_texture(MetalContext &context, MemState &mem, std::size_t index, SceGxmTexture texture, const Config &config);
void sync_viewport_flat(MetalContext &context);
void sync_viewport_real(MetalContext &context, const float xOffset, const float yOffset, const float zOffset,
    const float xScale, const float yScale, const float zScale);
void sync_visibility_buffer(MetalContext &context, Ptr<uint32_t> buffer, uint32_t stride);
void sync_visibility_index(MetalContext &context, bool enable, uint32_t index, bool is_increment);

void refresh_pipeline(MetalContext &context);

// executed on the render thread: switch the screen filter
void apply_screen_filter(MetalState &state, std::string_view filter);

} // namespace renderer::metal
