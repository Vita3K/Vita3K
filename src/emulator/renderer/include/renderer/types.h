#pragma once

#include "texture_cache_state.h"

#include <glutil/object.h>
#include <glutil/object_array.h>

#include <psp2/gxm.h>

#include <map>
#include <memory>
#include <tuple>

typedef void *SDL_GLContext;

namespace renderer {
    typedef std::map<GLuint, std::string> AttributeLocations;
    typedef std::unique_ptr<void, std::function<void(SDL_GLContext)>> GLContextPtr;
    typedef std::tuple<std::string, std::string> ProgramGLSLs;
    typedef std::shared_ptr<GLObject> SharedGLObject;
    typedef std::map<ProgramGLSLs, SharedGLObject> ProgramCache;

    struct Context {
        GLContextPtr gl;
        ProgramCache program_cache;
        TextureCacheState texture_cache;
        GLObjectArray<1> vertex_array;
        GLObjectArray<1> element_buffer;
        GLObjectArray<SCE_GXM_MAX_VERTEX_STREAMS> stream_vertex_buffers;
    };

    struct FragmentProgram {
        std::string glsl;
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

    struct RenderTarget {
        GLObjectArray<2> renderbuffers;
        GLObjectArray<1> framebuffer;
    };

    struct VertexProgram {
        std::string glsl;
        AttributeLocations attribute_locations;
    };
}
