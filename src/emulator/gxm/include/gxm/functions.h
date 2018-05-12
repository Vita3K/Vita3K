#pragma once

#include <glutil/gl.h>

#include <psp2/gxm.h>

#include <map>
#include <memory>

class GLObject;
struct MemState;

namespace emu {
    struct SceGxmBlendInfo;
}

struct FragmentProgramCacheKey;
typedef std::shared_ptr<GLObject> SharedGLObject;
typedef std::map<GLuint, std::string> AttributeLocations;

std::string get_fragment_glsl(SceGxmShaderPatcher &shader_patcher, const SceGxmProgram &fragment_program, const char *base_path);
std::string get_vertex_glsl(SceGxmShaderPatcher &shader_patcher, const SceGxmProgram &vertex_program, const char *base_path);
AttributeLocations attribute_locations(const SceGxmProgram &vertex_program);
SharedGLObject get_program(SceGxmContext &context, const MemState &mem);
GLenum attribute_format_to_gl_type(SceGxmAttributeFormat format);
size_t attribute_format_size(SceGxmAttributeFormat format);
bool attribute_format_normalised(SceGxmAttributeFormat format);
void set_uniforms(GLuint program, const SceGxmContext &context, const MemState &mem);
void flip_vertically(uint32_t *pixels, size_t width, size_t height, size_t stride_in_pixels);
GLenum translate_blend_func(SceGxmBlendFunc src);
GLenum translate_blend_factor(SceGxmBlendFactor src);
namespace texture {
    SceGxmTextureBaseFormat get_base_format(SceGxmTextureFormat src);
    bool is_paletted_format(SceGxmTextureFormat src);
    GLenum translate_internal_format(SceGxmTextureFormat src);
    GLenum translate_format(SceGxmTextureFormat src);
    GLenum translate_wrap_mode(SceGxmTextureAddrMode src);
    GLenum translate_type(SceGxmTextureFormat format);
    const GLenum *translate_swizzle(SceGxmTextureFormat fmt);
    GLenum translate_minmag_filter(SceGxmTextureFilter src);
    SceGxmTextureFormat get_format(const SceGxmTexture *texture);
    unsigned int get_width(const SceGxmTexture *texture);
    unsigned int get_height(const SceGxmTexture *texture);
    void palette_texture_to_rgba_4(uint32_t *dst, const uint8_t *src, size_t width, size_t height, const uint32_t *palette);
    void palette_texture_to_rgba_8(uint32_t *dst, const uint8_t *src, size_t width, size_t height, const uint32_t *palette);
}
GLenum translate_primitive(SceGxmPrimitiveType primType);
GLenum translate_stencil_func(SceGxmStencilFunc stencil_func);
GLenum translate_depth_func(SceGxmDepthFunc depth_func);
GLenum translate_stencil_op(SceGxmStencilOp stencil_op);
bool operator<(const FragmentProgramCacheKey &a, const FragmentProgramCacheKey &b);
