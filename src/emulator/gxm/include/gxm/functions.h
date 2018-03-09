#pragma once

#include <glutil/gl.h>

#include <psp2/gxm.h>

#include <map>
#include <memory>

namespace glbinding {
    struct FunctionCall;
}

class GLObject;

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
SharedGLObject get_program(SceGxmContext &context, const SceGxmFragmentProgram &fragment_program);
GLenum attribute_format_to_gl_type(SceGxmAttributeFormat format);
bool attribute_format_normalised(SceGxmAttributeFormat format);
void flip_vertically(uint32_t *pixels, size_t width, size_t height, size_t stride_in_pixels);
GLenum translate_blend_func(SceGxmBlendFunc src);
GLenum translate_blend_factor(SceGxmBlendFactor src);
GLenum translate_internal_format(SceGxmTextureFormat src);
GLenum translate_format(SceGxmTextureFormat src);
GLenum translate_primitive(SceGxmPrimitiveType primType);
bool operator<(const FragmentProgramCacheKey& a, const FragmentProgramCacheKey &b);
