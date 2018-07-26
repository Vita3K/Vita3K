#pragma once

#include <crypto/hash.h>
#include <glutil/object.h>

#include <psp2/gxm.h>

#include <map>
#include <string>
#include <tuple>

struct GxmContextState;
struct MemState;

namespace renderer {
    struct TextureCacheState;

    typedef std::map<Sha256Hash, std::string> GLSLCache;
    typedef std::tuple<std::string, std::string> ProgramGLSLs;
    typedef std::map<ProgramGLSLs, SharedGLObject> ProgramCache;

    // Attribute formats.
    size_t attribute_format_size(SceGxmAttributeFormat format);
    GLenum attribute_format_to_gl_type(SceGxmAttributeFormat format);
    GLboolean attribute_format_normalised(SceGxmAttributeFormat format);

    // Compile program.
    SharedGLObject compile_program(ProgramCache &cache, const GxmContextState &state, const MemState &mem);

    // Texture cache.
    bool init(TextureCacheState &cache);
    void cache_and_bind_texture(TextureCacheState &cache, const SceGxmTexture &gxm_texture, const MemState &mem, bool enabled);

    // Shaders.
    std::string load_fragment_shader(GLSLCache &cache, const SceGxmProgram &fragment_program, const char *base_path);
    std::string load_vertex_shader(GLSLCache &cache, const SceGxmProgram &vertex_program, const char *base_path);

    // Texture formats.
    const GLenum *translate_swizzle(SceGxmTextureFormat fmt);
    GLenum translate_internal_format(SceGxmTextureFormat src);
    GLenum translate_format(SceGxmTextureFormat src);
    GLenum translate_type(SceGxmTextureFormat format);

    // Uniforms.
    void set_uniforms(GLuint program, const GxmContextState &state, const MemState &mem);
}
