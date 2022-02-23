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

#include <renderer/functions.h>
#include <renderer/state.h>
#include <renderer/types.h>

#include <gxm/functions.h>

#include <cstring>

namespace renderer {
void set_depth_bias(State &state, Context *ctx, bool is_front, int factor, int units) {
    renderer::add_state_set_command(ctx, renderer::GXMState::DepthBias, is_front, factor, units);
}

void set_depth_func(State &state, Context *ctx, bool is_front, SceGxmDepthFunc depth_func) {
    renderer::add_state_set_command(ctx, renderer::GXMState::DepthFunc, is_front, depth_func);
}

void set_depth_write_enable_mode(State &state, Context *ctx, bool is_front, SceGxmDepthWriteMode enable) {
    renderer::add_state_set_command(ctx, renderer::GXMState::DepthWriteEnable, is_front, enable);
}

void set_point_line_width(State &state, Context *ctx, bool is_front, unsigned int width) {
    renderer::add_state_set_command(ctx, renderer::GXMState::PointLineWidth, is_front, width);
}

void set_polygon_mode(State &state, Context *ctx, bool is_front, SceGxmPolygonMode mode) {
    renderer::add_state_set_command(ctx, renderer::GXMState::PolygonMode, is_front, mode);
}

void set_stencil_func(State &state, Context *ctx, bool is_front, SceGxmStencilFunc func, SceGxmStencilOp stencilFail, SceGxmStencilOp depthFail, SceGxmStencilOp depthPass, unsigned char compareMask, unsigned char writeMask) {
    renderer::add_state_set_command(ctx, renderer::GXMState::StencilFunc, is_front, func, stencilFail, depthFail, depthPass, compareMask, writeMask);
}

void set_stencil_ref(State &state, Context *ctx, bool is_front, unsigned char sref) {
    renderer::add_state_set_command(ctx, renderer::GXMState::StencilRef, is_front, sref);
}

void set_program(State &state, Context *ctx, Ptr<const void> program, const bool is_fragment) {
    renderer::add_state_set_command(ctx, renderer::GXMState::Program, program, is_fragment);
}

void set_cull_mode(State &state, Context *ctx, SceGxmCullMode cull) {
    renderer::add_state_set_command(ctx, renderer::GXMState::CullMode, cull);
}

void set_texture(State &state, Context *ctx, const std::uint32_t tex_index, const SceGxmTexture tex) {
    renderer::add_state_set_command(ctx, renderer::GXMState::Texture, tex_index, tex);
}

void set_viewport_real(State &state, Context *ctx, float xOffset, float yOffset, float zOffset, float xScale, float yScale, float zScale) {
    renderer::add_state_set_command(ctx, renderer::GXMState::Viewport, false, xOffset, yOffset,
        zOffset, xScale, yScale, zScale);
}

void set_viewport_flat(State &state, Context *ctx) {
    renderer::add_state_set_command(ctx, renderer::GXMState::Viewport, true);
}

void set_region_clip(State &state, Context *ctx, SceGxmRegionClipMode mode, unsigned int xMin, unsigned int xMax, unsigned int yMin, unsigned int yMax) {
    renderer::add_state_set_command(ctx, renderer::GXMState::RegionClip, mode, xMin, xMax, yMin, yMax);
}

void set_two_sided_enable(State &state, Context *ctx, SceGxmTwoSidedMode mode) {
    renderer::add_state_set_command(ctx, renderer::GXMState::TwoSided, mode);
}

void set_side_fragment_program_enable(State &state, Context *ctx, const bool is_front, SceGxmFragmentProgramMode mode) {
    renderer::add_state_set_command(ctx, renderer::GXMState::FragmentProgramEnable, is_front, mode);
}

void set_context(State &state, Context *ctx, RenderTarget *target, SceGxmColorSurface *color_surface, SceGxmDepthStencilSurface *depth_stencil_surface) {
    renderer::add_command(ctx, renderer::CommandOpcode::SetContext, nullptr, target, color_surface, depth_stencil_surface);
}

std::uint8_t **set_vertex_stream(State &state, Context *ctx, const std::size_t index, const std::size_t data_len) {
    renderer::add_state_set_command(ctx, renderer::GXMState::VertexStream, nullptr, index, data_len);
    return reinterpret_cast<std::uint8_t **>(ctx->command_list.last->data + 2);
}

void draw(State &state, Context *ctx, SceGxmPrimitiveType prim_type, SceGxmIndexFormat index_type, const void *index_data, const std::uint32_t index_count, const std::uint32_t instance_count) {
    std::uint8_t *a_copy = new std::uint8_t[index_count * gxm::index_element_size(index_type)];

    std::memcpy(a_copy, index_data, index_count * gxm::index_element_size(index_type));
    renderer::add_command(ctx, renderer::CommandOpcode::Draw, nullptr, prim_type, index_type, a_copy, index_count, instance_count);
}

void sync_surface_data(State &state, Context *ctx) {
    renderer::add_command(ctx, renderer::CommandOpcode::SyncSurfaceData, nullptr);
}

bool create_context(State &state, std::unique_ptr<Context> &context) {
    return renderer::send_single_command(state, nullptr, renderer::CommandOpcode::CreateContext, &context);
}

bool create_render_target(State &state, std::unique_ptr<RenderTarget> &rt, const SceGxmRenderTargetParams *params) {
    return renderer::send_single_command(state, nullptr, renderer::CommandOpcode::CreateRenderTarget, &rt, params);
}

void destroy_render_target(State &state, std::unique_ptr<RenderTarget> &rt) {
    renderer::send_single_command(state, nullptr, renderer::CommandOpcode::DestroyRenderTarget, &rt);
}

void set_uniform(State &state, Context *ctx, const bool is_vertex_uniform, const SceGxmProgramParameter *parameter, const void *data) {
    renderer::add_state_set_command(ctx, renderer::GXMState::Uniform, is_vertex_uniform, parameter, data);
}

std::uint8_t **set_uniform_buffer(State &state, Context *ctx, const bool is_vertex_uniform, const int block_number, const std::uint16_t block_size) {
    // Calculate the number of bytes
    std::uint32_t bytes_to_copy_and_pad = (((block_size + 15) / 16)) * 16;

    renderer::add_state_set_command(ctx, renderer::GXMState::UniformBuffer, nullptr, is_vertex_uniform, block_number, bytes_to_copy_and_pad);
    return reinterpret_cast<std::uint8_t **>(ctx->command_list.last->data + 2);
}

} // namespace renderer
