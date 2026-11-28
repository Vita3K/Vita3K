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

#include <glutil/object.h>
#include <glutil/object_array.h>
#include <renderer/types.h>
#include <shader/spirv_recompiler.h>
#include <util/hash.h>

#include <renderer/gl/ring_buffer.h>
#include <renderer/texture_cache.h>
#include <shader/uniform_block.h>

#include <glad/glad.h>

#include <atomic>
#include <map>
#include <memory>
#include <string>
#include <vector>

// Our glad build does not expose GL_ARB_parallel_shader_compile / GL_KHR_parallel_shader_compile,
// so the token is defined manually here. Both extensions use the same value.
#ifndef GL_COMPLETION_STATUS_ARB
#define GL_COMPLETION_STATUS_ARB 0x91B1
#endif

struct SceGxmProgramParameter;

namespace renderer::gl {
struct ExcludedUniform {
    std::string name;
    GLuint program;
};

inline bool operator==(const ExcludedUniform &lhs, const ExcludedUniform &rhs) {
    return (lhs.name == rhs.name) && (lhs.program == rhs.program);
}

using ShaderCache = std::map<Sha256Hash, SharedGLObject>;
using ProgramHashes = std::tuple<Sha256Hash, Sha256Hash>;

enum class ProgramStatus {
    // glLinkProgram has been issued but completion has not been observed yet.
    // Only reachable when asynchronous compilation is enabled.
    Compiling,
    // The program is linked and can be used for drawing.
    Ready,
    // Linking failed. The entry is kept so that we do not endlessly retry it.
    Failed
};

struct CachedProgram {
    SharedGLObject program;
    ProgramStatus status = ProgramStatus::Compiling;
};

using ProgramCache = std::map<ProgramHashes, CachedProgram>;

// A program whose linking has been issued but not yet completed.
// The shaders are kept alive here because glDetachShader is deferred until completion.
struct PendingProgram {
    ProgramHashes hashes;
    SharedGLObject program;
    SharedGLObject frag_shader;
    SharedGLObject vert_shader;
};

struct ShaderTranslation;

// A program linked by the compile worker, linked again on the render thread once it is done
struct WorkerProgram {
    ProgramHashes hashes;
    std::shared_ptr<const ShaderTranslation> frag;
    std::shared_ptr<const ShaderTranslation> vert;
    bool spirv;
};

enum class ShaderStatus {
    // A worker thread is turning the gxp program into GLSL or SPIR-V.
    Translating,
    // The generated source is available and the GL shader object can be created.
    Ready,
    // Translation failed.
    Failed
};

// The result of translating one gxp program, shared between a worker thread and the
// render thread. Only the worker writes to it, and only before publishing the status.
struct ShaderTranslation {
    std::atomic<ShaderStatus> status{ ShaderStatus::Translating };

    std::string glsl;
    std::vector<uint32_t> spirv;
};

// Everything a worker thread needs to translate a shader without touching anything
// that the render thread may concurrently modify.
struct ShaderTranslateRequest {
    // Shared ownership: the entry may be dropped from the map while a worker still holds it,
    // for instance when asynchronous compilation is turned off from the UI thread.
    std::shared_ptr<ShaderTranslation> target;

    // The gxp program is guest memory. It is kept alive for the duration of the request
    // by the refcount below, which SceGxm waits on before destroying a program.
    const SceGxmProgram *program;
    std::atomic<uint32_t> *refcount;

    bool is_vertex;
    bool maskupdate;
    bool spirv;
    bool use_shader_cache;

    shader::Hints hints;
    // hints.attributes points here: the vertex program's own vector must not be
    // read from a worker thread.
    std::vector<SceGxmVertexAttribute> attributes;
};

typedef std::vector<ExcludedUniform> ExcludedUniforms; // vector instead of unordered_set since it's much faster for few elements
using UniformTypes = std::map<GLuint, GLenum>;

class GLTextureCache : public TextureCache {
public:
    GLObjectArray<TextureCacheSize> textures;
    std::array<GLenum, TextureCacheSize> targets{};
    // the compressed uploads must use the internal format of the texture
    std::array<GLenum, TextureCacheSize> internal_formats{};
    size_t current_index = 0;
    bool support_srgb_r8 = false;
    bool support_srgb_rg8 = false;

    GLenum linear_to_srgb(GLenum format) const;

    bool init(const bool hashless_texture_cache, const fs::path &texture_folder, const std::string_view game_id);
    void cleanup();
    void select(size_t index, const SceGxmTexture &texture) override;
    void configure_texture(const SceGxmTexture &texture) override;
    void upload_texture_impl(SceGxmTextureBaseFormat base_format, uint32_t width, uint32_t height, uint32_t mip_index, const void *pixels, int face, uint32_t pixels_per_stride) override;

    void import_configure_impl(SceGxmTextureBaseFormat base_format, uint32_t width, uint32_t height, bool is_srgb, uint16_t nb_components, uint16_t mipcount, bool swap_rb) override;
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
    // fragment texture units that filter the self-sampled color attachment through its mips
    std::uint32_t self_sampling_mipmap_units = 0;
    // indexed by the gamma mode of the texture
    GLObjectArray<2> point_surface_samplers;

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
