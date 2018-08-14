#pragma once

#include "texture_cache_state.h"

#include <glutil/object.h>
#include <glutil/object_array.h>

#include <psp2/gxm.h>

#include <map>
#include <memory>
#include <tuple>
#include <vector>

typedef void *SDL_GLContext;

namespace renderer {
typedef std::map<GLuint, std::string> AttributeLocations;
typedef std::unique_ptr<void, std::function<void(SDL_GLContext)>> GLContextPtr;
typedef std::tuple<std::string, std::string> ProgramGLSLs;
typedef std::map<ProgramGLSLs, SharedGLObject> ProgramCache;
typedef std::vector<std::string> ExcludedUniforms; // vector instead of unordered_set since it's much faster for few elements

struct Context {
    GLContextPtr gl;
    ProgramCache program_cache;
    TextureCacheState texture_cache;
    GLObjectArray<1> vertex_array;
    GLObjectArray<1> element_buffer;
    GLObjectArray<SCE_GXM_MAX_VERTEX_STREAMS> stream_vertex_buffers;
};

struct ShaderProgram {
    std::string glsl;
    ExcludedUniforms excluded_uniforms;
};

struct FragmentProgram : ShaderProgram {
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

struct VertexProgram : ShaderProgram {
    AttributeLocations attribute_locations;
};

struct RenderTarget {
    GLObjectArray<2> renderbuffers;
    GLObjectArray<1> framebuffer;
};
} // namespace renderer
