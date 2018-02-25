#pragma once

#include <glutil/gl.h>

#include <psp2/gxm.h>

namespace glbinding {
    struct FunctionCall;
}

struct ReportingState;

void before_callback(const glbinding::FunctionCall &fn);
void after_callback(const glbinding::FunctionCall &fn);
bool compile_fragment_shader(GLuint shader, const SceGxmProgram &program, const char *base_path, ReportingState &reporting);
bool compile_vertex_shader(GLuint shader, const SceGxmProgram &program, const char *base_path, ReportingState &reporting);
GLenum attribute_format_to_gl_type(SceGxmAttributeFormat format);
bool attribute_format_normalised(SceGxmAttributeFormat format);
void bind_attribute_locations(GLuint gl_program, const SceGxmProgram &program);
void flip_vertically(uint32_t *pixels, size_t width, size_t height, size_t stride_in_pixels);
GLenum translate_blend_func(SceGxmBlendFunc src);
GLenum translate_blend_factor(SceGxmBlendFactor src);
GLenum translate_internal_format(SceGxmTextureFormat src);
GLenum translate_format(SceGxmTextureFormat src);
GLenum translate_primitive(SceGxmPrimitiveType primType);
