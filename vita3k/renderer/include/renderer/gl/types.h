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

#include <crypto/hash.h>
#include <glutil/object.h>
#include <glutil/object_array.h>
#include <renderer/types.h>

#include <renderer/gl/ring_buffer.h>
#include <renderer/texture_cache_state.h>
#include <shader/usse_program_analyzer.h>

#include <map>
#include <memory>
#include <set>
#include <tuple>
#include <vector>

typedef void *SDL_GLContext;

typedef std::unique_ptr<void, std::function<void(SDL_GLContext)>> GLContextPtr;

struct SceGxmProgramParameter;

namespace renderer::gl {
struct ExcludedUniform {
    std::string name;
    GLuint program;
};

inline bool operator==(const ExcludedUniform &lhs, const ExcludedUniform &rhs) {
    return (lhs.name == rhs.name) && (lhs.program == rhs.program);
}

typedef std::map<Sha256Hash, SharedGLObject> ShaderCache;
typedef std::map<ProgramHashes, SharedGLObject> ProgramCache;
typedef std::vector<ExcludedUniform> ExcludedUniforms; // vector instead of unordered_set since it's much faster for few elements
typedef std::map<GLuint, GLenum> UniformTypes;

struct GLTextureCacheState : public renderer::TextureCacheState {
    GLObjectArray<TextureCacheSize> textures;
};

struct GLRenderTarget;

struct GLContext : public renderer::Context {
    GLObjectArray<1> vertex_array;

    RingBuffer vertex_stream_ring_buffer;
    RingBuffer index_stream_ring_buffer;
    RingBuffer vertex_uniform_stream_ring_buffer;
    RingBuffer fragment_uniform_stream_ring_buffer;
    RingBuffer vertex_info_uniform_buffer;
    RingBuffer fragment_info_uniform_buffer;

    const GLRenderTarget *render_target{};

    GLObjectArray<SCE_GXM_MAX_VERTEX_STREAMS> stream_vertex_buffers;
    GLuint last_draw_program{ 0 };
    GLuint current_framebuffer{ 0 };
    GLuint current_color_attachment{ 0 };
    GLuint current_framebuffer_height{ 0 };

    std::pair<std::uint8_t *, std::size_t> vertex_uniform_buffer_storage_ptr{ nullptr, 0 };
    std::pair<std::uint8_t *, std::size_t> fragment_uniform_buffer_storage_ptr{ nullptr, 0 };

    shader::RenderVertUniformBlock previous_vert_info;
    shader::RenderFragUniformBlock previous_frag_info;

    shader::RenderVertUniformBlock current_vert_render_info{};
    shader::RenderFragUniformBlock current_frag_render_info{};

    std::vector<size_t> self_sampling_indices;

    explicit GLContext();
    ~GLContext() override = default;
};

struct GLShaderStatics {
    ExcludedUniforms excluded_uniforms;
    UniformTypes uniform_types;
};

struct GLFragmentProgram : public renderer::FragmentProgram {
    GLShaderStatics statics;
    GLboolean color_mask_red = GL_TRUE;
    GLboolean color_mask_green = GL_TRUE;
    GLboolean color_mask_blue = GL_TRUE;
    GLboolean color_mask_alpha = GL_TRUE;
    bool blend_enabled = false;
    GLenum color_func = GL_FUNC_ADD;
    GLenum alpha_func = GL_FUNC_ADD;
    GLenum color_src = GL_ONE;
    GLenum color_dst = GL_ZERO;
    GLenum alpha_src = GL_ONE;
    GLenum alpha_dst = GL_ZERO;
};

struct GLVertexProgram : public renderer::VertexProgram {
    GLShaderStatics statics;
};

struct GLRenderTarget : public renderer::RenderTarget {
    uint16_t width;
    uint16_t height;
    GLObjectArray<1> maskbuffer;
    GLObjectArray<1> masktexture;
    GLObjectArray<2> attachments;

    ~GLRenderTarget() override = default;
};
} // namespace renderer::gl
