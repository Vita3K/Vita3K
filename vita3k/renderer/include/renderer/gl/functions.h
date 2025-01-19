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

#include <config/state.h>
#include <glutil/object.h>
#include <gxm/types.h>

#include <renderer/gl/state.h>
#include <renderer/gl/types.h>

#include <memory>

struct MemState;
struct FeatureState;

namespace renderer::gl {

// Compile program.
SharedGLObject compile_program(GLState &renderer, GLContext &context, const GxmRecordState &state, const FeatureState &features, const MemState &mem, bool shader_cache, bool spirv, bool maskupdate);
void pre_compile_program(GLState &renderer, const ShadersHash &hashs);

// Uniforms.
bool set_uniform_buffer(GLContext &context, const ShaderProgram *program, const bool vertex_shader, const int block_num, const int size, const uint8_t *data);

bool create(SDL_Window *window, std::unique_ptr<renderer::State> &state, const Config &config);
bool create(std::unique_ptr<Context> &context);
bool create(GLState &state, std::unique_ptr<RenderTarget> &rt, const SceGxmRenderTargetParams &params, const FeatureState &features);
bool create(std::unique_ptr<FragmentProgram> &fp, GLState &state, const SceGxmProgram &program, const SceGxmBlendInfo *blend);
bool create(std::unique_ptr<VertexProgram> &vp, GLState &state, const SceGxmProgram &program);
void set_context(GLState &state, GLContext &ctx, const MemState &mem, const GLRenderTarget *rt, const FeatureState &features);
void get_surface_data(GLState &renderer, GLContext &context, uint32_t *pixels, SceGxmColorSurface &surface);
void lookup_and_get_surface_data(GLState &renderer, MemState &mem, SceGxmColorSurface &surface);
void draw(GLState &renderer, GLContext &context, const FeatureState &features, SceGxmPrimitiveType type, SceGxmIndexFormat format,
    void *indices, size_t count, uint32_t instance_count, MemState &mem, const Config &config);

// State
void sync_viewport_flat(const GLState &state, GLContext &context);
void sync_viewport_real(const GLState &state, GLContext &context, const float xOffset, const float yOffset, const float zOffset,
    const float xScale, const float yScale, const float zScale);

void sync_clipping(const GLState &state, GLContext &context);
void sync_cull(const GxmRecordState &state);
void sync_depth_func(const SceGxmDepthFunc func, const bool is_front);
void sync_depth_write_enable(const SceGxmDepthWriteMode mode, const bool is_front);
void sync_depth_data(const renderer::GxmRecordState &state);
void sync_stencil_data(const renderer::GxmRecordState &state, const MemState &mem);
void sync_stencil_func(const GxmStencilStateOp &state_op, const GxmStencilStateValues &state_vals, const MemState &mem, const bool is_back_stencil);
void sync_mask(const GLState &state, GLContext &context, const MemState &mem);
void sync_polygon_mode(const SceGxmPolygonMode mode, const bool front);
void sync_point_line_width(const GLState &state, const std::uint32_t size, const bool front);
void sync_depth_bias(const int factor, const int unit, const bool front);
void sync_blending(const GxmRecordState &state, const MemState &mem);
void sync_texture(GLState &state, GLContext &context, MemState &mem, std::size_t index, SceGxmTexture texture, const Config &config);
void sync_vertex_streams_and_attributes(GLContext &context, GxmRecordState &state, const MemState &mem);
void bind_fundamental(GLContext &context);
void clear_previous_uniform_storage(GLContext &context);

struct GLTextureCacheState;

// Attribute formats.
GLenum attribute_format_to_gl_type(SceGxmAttributeFormat format);
GLboolean attribute_format_normalized(SceGxmAttributeFormat format);

namespace texture {

// Textures.
void bind_texture_without_cache(GLTextureCache &cache, const SceGxmTexture &gxm_texture, MemState &mem);

// Texture formats.
const GLint *translate_swizzle(SceGxmTextureFormat fmt);
GLenum translate_internal_format(SceGxmTextureBaseFormat base_format);
GLenum translate_format(SceGxmTextureBaseFormat base_format);
GLenum translate_type(SceGxmTextureBaseFormat base_format);
GLenum translate_wrap_mode(SceGxmTextureAddrMode src);
GLenum translate_minmag_filter(SceGxmTextureFilter src);
GLenum get_gl_texture_type(const SceGxmTexture &gxm_texture);
size_t bits_per_pixel(SceGxmTextureBaseFormat base_format);

// Texture cache.
bool init(GLTextureCacheState &cache, const bool hashless_texture_cache);

} // namespace texture

namespace color {

GLenum translate_format(SceGxmColorBaseFormat base_format);
GLenum translate_internal_format(SceGxmColorBaseFormat base_format);
GLenum translate_type(SceGxmColorBaseFormat base_format);
const GLint *translate_swizzle(SceGxmColorFormat fmt);
size_t bytes_per_pixel(SceGxmColorBaseFormat base_format);
size_t bytes_per_pixel_in_gl_storage(SceGxmColorBaseFormat base_format);
bool is_write_surface_stored_rawly(SceGxmColorBaseFormat base_format);
bool is_write_surface_non_linearity_filtering(SceGxmColorBaseFormat base_format);
GLenum get_raw_store_internal_type(SceGxmColorBaseFormat base_format);
GLenum get_raw_store_upload_format_type(SceGxmColorBaseFormat base_format);
GLenum get_raw_store_upload_data_type(SceGxmColorBaseFormat base_format);

} // namespace color

} // namespace renderer::gl
