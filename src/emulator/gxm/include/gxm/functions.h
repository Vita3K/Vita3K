#pragma once

#include <glutil/gl.h>

#include <psp2/gxm.h>

#include <map>
#include <memory>

namespace glbinding {
    struct FunctionCall;
}

class GLObject;
struct MemState;

namespace emu {
    struct SceGxmBlendInfo;
}

struct FragmentProgramCacheKey;
typedef std::shared_ptr<GLObject> SharedGLObject;
typedef std::map<GLuint, std::string> AttributeLocations;

void before_callback(const glbinding::FunctionCall &fn);
void after_callback(const glbinding::FunctionCall &fn);
std::string get_fragment_glsl(SceGxmShaderPatcher &shader_patcher, const SceGxmProgram &fragment_program, const char *base_path);
std::string get_vertex_glsl(SceGxmShaderPatcher &shader_patcher, const SceGxmProgram &vertex_program, const char *base_path);
AttributeLocations attribute_locations(const SceGxmProgram &vertex_program);
SharedGLObject get_program(SceGxmContext &context, const MemState &mem);
GLenum attribute_format_to_gl_type(SceGxmAttributeFormat format);
bool attribute_format_normalised(SceGxmAttributeFormat format);
void set_uniforms(GLuint program, const SceGxmContext &context, const MemState &mem);
void flip_vertically(uint32_t *pixels, size_t width, size_t height, size_t stride_in_pixels);
SceGxmTextureFormat get_texture_format(SceGxmTexture *texture);
unsigned int get_texture_width(SceGxmTexture *texture);
unsigned int get_texture_height(SceGxmTexture *texture);
GLenum translate_blend_func(SceGxmBlendFunc src);
GLenum translate_blend_factor(SceGxmBlendFactor src);
namespace texture {
    SceGxmTextureBaseFormat get_base_format(SceGxmTextureFormat src);
    std::uint32_t get_swizzle(SceGxmTextureFormat src);
    bool is_paletted_format(SceGxmTextureFormat src);
    GLenum translate_internal_format(SceGxmTextureFormat src);
    GLenum translate_format(SceGxmTextureFormat src);
}
GLenum translate_primitive(SceGxmPrimitiveType primType);
bool operator<(const FragmentProgramCacheKey& a, const FragmentProgramCacheKey &b);
