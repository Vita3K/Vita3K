#pragma once

#include <glutil/gl.h>

#include <psp2/gxm.h>

#include <memory>

namespace glbinding {
    struct FunctionCall;
}

class GLObject;
struct ReportingState;

typedef std::shared_ptr<GLObject> SharedGLObject;

void before_callback(const glbinding::FunctionCall &fn);
void after_callback(const glbinding::FunctionCall &fn);
SharedGLObject get_fragment_shader(SceGxmShaderPatcher &shader_patcher, ReportingState &reporting, const SceGxmProgram &fragment_program, const char *base_path);
SharedGLObject get_vertex_shader(SceGxmShaderPatcher &shader_patcher, ReportingState &reporting, const SceGxmProgram &vertex_program, const char *base_path);
GLenum attribute_format_to_gl_type(SceGxmAttributeFormat format);
bool attribute_format_normalised(SceGxmAttributeFormat format);
void bind_attribute_locations(GLuint gl_program, const SceGxmProgram &program);
void flip_vertically(uint32_t *pixels, size_t width, size_t height, size_t stride_in_pixels);
GLenum translate_blend_func(SceGxmBlendFunc src);
GLenum translate_blend_factor(SceGxmBlendFactor src);
GLenum translate_internal_format(SceGxmTextureFormat src);
GLenum translate_format(SceGxmTextureFormat src);
GLenum translate_primitive(SceGxmPrimitiveType primType);
