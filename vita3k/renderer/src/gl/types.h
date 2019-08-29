#pragma once

#include <crypto/hash.h>
#include <glutil/object.h>
#include <glutil/object_array.h>
#include <renderer/types.h>

#include <psp2/gxm.h>

#include <renderer/texture_cache_state.h>

#include <map>
#include <memory>
#include <tuple>
#include <vector>

typedef void *SDL_GLContext;

typedef std::unique_ptr<void, std::function<void(SDL_GLContext)>> GLContextPtr;

struct SceGxmProgramParameter;

namespace renderer::gl {
static constexpr GLint COLOR_ATTACHMENT_TEXTURE_SLOT_IMAGE = 0; ///< The slot that has our color attachment (for programmable blending) - image2D.
static constexpr GLint COLOR_ATTACHMENT_TEXTURE_SLOT_SAMPLER = 12; ///< The slot that has our color attachment (for programmable blending) - sampler2D.

struct ExcludedUniform {
    std::string name;
    GLuint program;
};

inline bool operator==(const ExcludedUniform &lhs, const ExcludedUniform &rhs) {
    return (lhs.name == rhs.name) && (lhs.program == rhs.program);
}

typedef std::map<GLuint, std::string> AttributeLocations;
typedef std::map<std::string, SharedGLObject> ShaderCache;
typedef std::tuple<std::string, std::string> ProgramHashes;
typedef std::map<ProgramHashes, SharedGLObject> ProgramCache;
typedef std::vector<ExcludedUniform> ExcludedUniforms; // vector instead of unordered_set since it's much faster for few elements
typedef std::map<GLuint, GLenum> UniformTypes;

struct UniformSetRequest {
    const SceGxmProgramParameter *parameter;
    const void *data;
};

struct GLTextureCacheState : public renderer::TextureCacheState {
    GLObjectArray<TextureCacheSize> textures;
};

struct GLContext : public renderer::Context {
    GLTextureCacheState texture_cache;
    GLObjectArray<1> vertex_array;
    GLObjectArray<1> element_buffer;
    GLObjectArray<SCE_GXM_MAX_VERTEX_STREAMS> stream_vertex_buffers;
    GLuint last_draw_program{ 0 };

    float viewport_flip[4] = { 1.0f, 1.0f, 1.0f, 1.0f };

    std::vector<UniformSetRequest> vertex_set_requests;
    std::vector<UniformSetRequest> fragment_set_requests;
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
    AttributeLocations attribute_locations;
};

struct GLRenderTarget : public renderer::RenderTarget {
    GLObjectArray<2> renderbuffers;
    GLObjectArray<1> framebuffer;
    GLObjectArray<1> color_attachment;
};
} // namespace renderer::gl
