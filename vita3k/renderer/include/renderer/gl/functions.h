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

#include <config/state.h>
#include <crypto/hash.h>
#include <glutil/object.h>
#include <gxm/types.h>

#include <renderer/gl/state.h>
#include <renderer/gl/types.h>

#include <map>
#include <memory>
#include <string>
#include <tuple>

struct MemState;
struct FeatureState;

namespace renderer::gl {

// Compile program.
SharedGLObject compile_program(GLState &renderer, const GxmRecordState &state, const FeatureState &features, const MemState &mem, bool shader_cache, bool spirv, bool maskupdate, const char *base_path, const char *title_id, const char *self_name);
void pre_compile_program(GLState &renderer, const char *base_path, const char *title_id, const char *self_name, const ShadersHash &hashs);

// Shaders.
bool get_shaders_cache_hashs(GLState &renderer, const char *base_path, const char *title_id, const char *self_name);
std::string load_glsl_shader(const SceGxmProgram &program, const FeatureState &features, const std::vector<SceGxmVertexAttribute> *hint_attributes, bool maskupdate, const char *base_path, const char *title_id, const char *self_name, const std::string &shader_version, bool shader_cache);
std::vector<std::uint32_t> load_spirv_shader(const SceGxmProgram &program, const FeatureState &features, const std::vector<SceGxmVertexAttribute> *hint_attributes, bool maskupdate, const char *base_path, const char *title_id, const char *self_name);
std::string pre_load_glsl_shader(const char *hash_text, const char *shader_type_str, const char *base_path, const char *title_id, const char *self_name);

// Uniforms.
bool set_uniform_buffer(GLContext &context, MemState &mem, const bool vertex_shader, const int block_num, const int size, const void *data, bool log_active_shader);

bool create(SDL_Window *window, std::unique_ptr<renderer::State> &state, const bool hashless_texture_cache);
bool create(std::unique_ptr<Context> &context);
bool create(std::unique_ptr<RenderTarget> &rt, const SceGxmRenderTargetParams &params, const FeatureState &features);
bool create(std::unique_ptr<FragmentProgram> &fp, GLState &state, const SceGxmProgram &program, const SceGxmBlendInfo *blend, GXPPtrMap &gxp_ptr_map, const char *base_path, const char *title_id);
bool create(std::unique_ptr<VertexProgram> &vp, GLState &state, const SceGxmProgram &program, GXPPtrMap &gxp_ptr_map, const char *base_path, const char *title_id);
void sync_rendertarget(const GLRenderTarget &rt);
void set_context(GLState &state, GLContext &ctx, const MemState &mem, const GLRenderTarget *rt, const FeatureState &features);
void get_surface_data(GLState &renderer, GLContext &context, size_t width, size_t height, size_t stride_in_pixels, uint32_t *pixels, SceGxmColorFormat format);
void draw(GLState &renderer, GLContext &context, const FeatureState &features, SceGxmPrimitiveType type, SceGxmIndexFormat format,
    void *indices, size_t count, uint32_t instance_count, MemState &mem, const char *base_path, const char *title_id, const char *self_name, const Config &config);

// State
void sync_viewport_flat(GLContext &context);
void sync_viewport_real(GLContext &context, const float xOffset, const float yOffset, const float zOffset,
    const float xScale, const float yScale, const float zScale);

void sync_clipping(GLContext &context);
void sync_cull(const GxmRecordState &state);
void sync_depth_func(const SceGxmDepthFunc func, const bool is_front);
void sync_depth_write_enable(const SceGxmDepthWriteMode mode, const bool is_front);
bool sync_depth_data(const renderer::GxmRecordState &state);
bool sync_stencil_data(const renderer::GxmRecordState &state, const MemState &mem);
void sync_stencil_func(const GxmStencilState &state, const MemState &mem, bool is_back_stencil);
void sync_mask(GLContext &context, const MemState &mem);
void sync_polygon_mode(const SceGxmPolygonMode mode, const bool front);
void sync_point_line_width(const std::uint32_t size, const bool front);
void sync_depth_bias(const int factor, const int unit, const bool front);
void sync_blending(const GxmRecordState &state, const MemState &mem);
void sync_texture(GLState &state, GLContext &context, MemState &mem, std::size_t index, SceGxmTexture texture, const Config &config,
    const std::string &base_path, const std::string &title_id);
void sync_vertex_streams_and_attributes(GLContext &context, GxmRecordState &state, const MemState &mem);
void bind_fundamental(GLContext &context);
void clear_previous_uniform_storage(GLContext &context);

struct GLTextureCacheState;
struct TextureCacheState;

// Attribute formats.
size_t attribute_format_size(SceGxmAttributeFormat format);
GLenum attribute_format_to_gl_type(SceGxmAttributeFormat format);
GLboolean attribute_format_normalised(SceGxmAttributeFormat format);

namespace texture {

// Textures.
void bind_texture(GLTextureCacheState &cache, const SceGxmTexture &gxm_texture, const MemState &mem);
void configure_bound_texture(const SceGxmTexture &gxm_texture);
void upload_bound_texture(const SceGxmTexture &gxm_texture, const MemState &mem);

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
void dump(const SceGxmTexture &gxm_texture, const MemState &mem, const std::string &name, const std::string &base_path, const std::string &title_id, Sha256Hash hash);

} // namespace texture
} // namespace renderer::gl
