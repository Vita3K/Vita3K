#include <renderer/functions.h>

#include "profile.h"

#include <renderer/texture_cache_functions.h>
#include <renderer/types.h>

#include <glutil/gl.h>
#include <gxm/functions.h>
#include <gxm/types.h>
#include <util/log.h>

#include <glbinding/Binding.h>
#include <SDL_video.h>

#include <cassert>

using namespace glbinding;

namespace renderer {
    static SharedGLObject compile_glsl(GLenum type, const std::string &source) {
        R_PROFILE(__func__);
    
        const SharedGLObject shader = std::make_shared<GLObject>();
        if (!shader->init(glCreateShader(type), glDeleteShader)) {
            return SharedGLObject();
        }
    
        const GLchar *source_glchar = static_cast<const GLchar *>(source.c_str());
        const GLint length = static_cast<GLint>(source.length());
        glShaderSource(shader->get(), 1, &source_glchar, &length);
    
        glCompileShader(shader->get());
    
        GLint log_length = 0;
        glGetShaderiv(shader->get(), GL_INFO_LOG_LENGTH, &log_length);
    
        if (log_length > 0) {
            std::vector<GLchar> log;
            log.resize(log_length);
            glGetShaderInfoLog(shader->get(), log_length, nullptr, log.data());
    
            LOG_ERROR("{}", log.data());
        }
    
        GLboolean is_compiled = GL_FALSE;
        glGetShaderiv(shader->get(), GL_COMPILE_STATUS, &is_compiled);
        assert(is_compiled != GL_FALSE);
        if (!is_compiled) {
            return SharedGLObject();
        }
    
        return shader;
    }
    
    static void bind_attribute_locations(GLuint gl_program, const VertexProgram &program) {
        R_PROFILE(__func__);
    
        for (const AttributeLocations::value_type &binding : program.attribute_locations) {
            glBindAttribLocation(gl_program, binding.first / sizeof(uint32_t), binding.second.c_str());
        }
    }
    
    static void flip_vertically(uint32_t *pixels, size_t width, size_t height, size_t stride_in_pixels) {
        R_PROFILE(__func__);
    
        uint32_t *row1 = &pixels[0];
        uint32_t *row2 = &pixels[(height - 1) * stride_in_pixels];
    
        while (row1 < row2) {
            std::swap_ranges(&row1[0], &row1[width], &row2[0]);
            row1 += stride_in_pixels;
            row2 -= stride_in_pixels;
        }
    }
    
    static SharedGLObject get_program(Context &context, const GxmContextState &state, const MemState &mem) {
        R_PROFILE(__func__);
    
        assert(state.fragment_program);
        assert(state.vertex_program);
    
        const FragmentProgram &fragment_program = *state.fragment_program.get(mem)->renderer.get();
        const VertexProgram &vertex_program = *state.vertex_program.get(mem)->renderer.get();
        const ProgramGLSLs glsls(fragment_program.glsl, vertex_program.glsl);
        const ProgramCache::const_iterator cached = context.program_cache.find(glsls);
        if (cached != context.program_cache.end()) {
            return cached->second;
        }
    
        const SharedGLObject fragment_shader = compile_glsl(GL_FRAGMENT_SHADER, fragment_program.glsl);
        if (!fragment_shader) {
            LOG_CRITICAL("Error in compiled fragment shader:\n{}", fragment_program.glsl);
            return SharedGLObject();
        }
        const SharedGLObject vertex_shader = compile_glsl(GL_VERTEX_SHADER, vertex_program.glsl);
        if (!vertex_shader) {
            LOG_CRITICAL("Error in compiled vertex shader:\n{}", vertex_program.glsl);
            return SharedGLObject();
        }
    
        const SharedGLObject program = std::make_shared<GLObject>();
        if (!program->init(glCreateProgram(), &glDeleteProgram)) {
            return SharedGLObject();
        }
    
        glAttachShader(program->get(), fragment_shader->get());
        glAttachShader(program->get(), vertex_shader->get());
    
        bind_attribute_locations(program->get(), vertex_program);
    
        glLinkProgram(program->get());
    
        GLint log_length = 0;
        glGetProgramiv(program->get(), GL_INFO_LOG_LENGTH, &log_length);
    
        if (log_length > 0) {
            std::vector<GLchar> log;
            log.resize(log_length);
            glGetProgramInfoLog(program->get(), log_length, nullptr, log.data());
    
            LOG_ERROR("{}\n", log.data());
        }
    
        GLboolean is_linked = GL_FALSE;
        glGetProgramiv(program->get(), GL_LINK_STATUS, &is_linked);
        assert(is_linked != GL_FALSE);
        if (is_linked == GL_FALSE) {
            return SharedGLObject();
        }
    
        glDetachShader(program->get(), fragment_shader->get());
        glDetachShader(program->get(), vertex_shader->get());
    
        context.program_cache.emplace(glsls, program);
    
        return program;
    }
    
    template <class T>
    static void uniform_4(GLint location, GLsizei count, const T *value);
    
    template <>
    void uniform_4<GLfloat>(GLint location, GLsizei count, const GLfloat *value) {
        glUniform4fv(location, count, value);
    }
    
    template <>
    void uniform_4<GLint>(GLint location, GLsizei count, const GLint *value) {
        glUniform4iv(location, count, value);
    }
    
    template <class T>
    static void uniform_1(GLint location, GLsizei count, const T *value);
    
    template <>
    void uniform_1<GLfloat>(GLint location, GLsizei count, const GLfloat *value) {
        glUniform1fv(location, count, value);
    }
    
    template <>
    void uniform_1<GLint>(GLint location, GLsizei count, const GLint *value) {
        glUniform1iv(location, count, value);
    }
    
    template <class T>
    static void uniform_matrix_4(GLint location, GLsizei count, GLboolean, const T *value);
    
    template <>
    void uniform_matrix_4<GLfloat>(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value) {
        glUniformMatrix4fv(location, count, transpose, value);
    }
    
    template <>
    void uniform_matrix_4<GLint>(GLint location, GLsizei count, GLboolean transpose, const GLint *value) {
        glUniform4iv(location, count, (GLint *)value);
    }
    
    template <class T>
    static void set_uniform(GLint location, size_t component_count, GLsizei array_size, const T *value) {
        switch (component_count) {
        case 1:
            switch (array_size) {
            case 1:
                uniform_1<T>(location, array_size, value);
                break;
            }
            break;
        case 4:
            if (array_size % 4 == 0) {
                uniform_matrix_4<T>(location, array_size / 4, GL_FALSE, value);
            } else {
                uniform_4<T>(location, array_size, value);
            }
            break;
    
        default:
            LOG_WARN("Unhandled uniform component count {}.", component_count);
            break;
        }
    }
    
    static void set_uniforms(GLuint gl_program, const UniformBuffers &uniform_buffers, const SceGxmProgram &gxm_program, const MemState &mem) {
        R_PROFILE(__func__);
    
        const SceGxmProgramParameter *const parameters = program_parameters(gxm_program);
        for (size_t i = 0; i < gxm_program.parameter_count; ++i) {
            const SceGxmProgramParameter &parameter = parameters[i];
            if (parameter.category != SCE_GXM_PARAMETER_CATEGORY_UNIFORM) {
                continue;
            }
    
            auto name = parameter_name(parameter);
            const GLint location = glGetUniformLocation(gl_program, name.c_str());
            if (location < 0) {
                LOG_WARN("Uniform parameter {} not found in current OpenGL program.", name);
                continue;
            }
    
            const SceGxmParameterType type = static_cast<SceGxmParameterType>(static_cast<uint16_t>(parameter.type));
            const Ptr<const void> uniform_buffer = uniform_buffers[parameter.container_index];
            if (!uniform_buffer) {
                LOG_WARN("Uniform buffer {} not set for parameter {}.", parameter.container_index, name);
                continue;
            }
    
            const GLint *src_s32;
            const GLfloat *src_f32;
    
            const uint8_t *const base = static_cast<const uint8_t *>(uniform_buffer.get(mem));
            switch (type) {
            case SCE_GXM_PARAMETER_TYPE_S32:
                src_s32 = reinterpret_cast<const GLint *>(base + parameter.resource_index * 4); // TODO What offset?
                set_uniform<GLint>(location, parameter.component_count, parameter.array_size, src_s32);
                break;
            case SCE_GXM_PARAMETER_TYPE_F32:
                src_f32 = reinterpret_cast<const GLfloat *>(base + parameter.resource_index * 4); // TODO What offset?
                set_uniform<GLfloat>(location, parameter.component_count, parameter.array_size, src_f32);
                break;
            default:
                LOG_WARN("Type {} not handled for uniform parameter {}.", type, name);
                break;
            }
        }
    }
    
    static void set_uniforms(GLuint program, const Context &context, const GxmContextState &state, const MemState &mem) {
        R_PROFILE(__func__);
    
        assert(state.fragment_program);
        assert(state.vertex_program);
    
        const SceGxmFragmentProgram &fragment_program = *state.fragment_program.get(mem);
        const SceGxmVertexProgram &vertex_program = *state.vertex_program.get(mem);
        assert(fragment_program.program);
        assert(vertex_program.program);
    
        set_uniforms(program, state.fragment_uniform_buffers, *fragment_program.program.get(mem), mem);
        set_uniforms(program, state.vertex_uniform_buffers, *vertex_program.program.get(mem), mem);
    }
    
    static GLenum translate_depth_func(SceGxmDepthFunc depth_func) {
        R_PROFILE(__func__);
        
        switch (depth_func) {
            case SCE_GXM_DEPTH_FUNC_NEVER:
                return GL_NEVER;
            case SCE_GXM_DEPTH_FUNC_LESS:
                return GL_LESS;
            case SCE_GXM_DEPTH_FUNC_EQUAL:
                return GL_EQUAL;
            case SCE_GXM_DEPTH_FUNC_LESS_EQUAL:
                return GL_LEQUAL;
            case SCE_GXM_DEPTH_FUNC_GREATER:
                return GL_GREATER;
            case SCE_GXM_DEPTH_FUNC_NOT_EQUAL:
                return GL_NOTEQUAL;
            case SCE_GXM_DEPTH_FUNC_GREATER_EQUAL:
                return GL_GEQUAL;
            case SCE_GXM_DEPTH_FUNC_ALWAYS:
                return GL_ALWAYS;
        }
        
        return GL_ALWAYS;
    }

    static GLenum translate_stencil_op(SceGxmStencilOp stencil_op) {
        R_PROFILE(__func__);
    
        switch (stencil_op) {
        case SCE_GXM_STENCIL_OP_KEEP:
            return GL_KEEP;
        case SCE_GXM_STENCIL_OP_ZERO:
            return GL_ZERO;
        case SCE_GXM_STENCIL_OP_REPLACE:
            return GL_REPLACE;
        case SCE_GXM_STENCIL_OP_INCR:
            return GL_INCR;
        case SCE_GXM_STENCIL_OP_DECR:
            return GL_DECR;
        case SCE_GXM_STENCIL_OP_INVERT:
            return GL_INVERT;
        case SCE_GXM_STENCIL_OP_INCR_WRAP:
            return GL_INCR_WRAP;
        case SCE_GXM_STENCIL_OP_DECR_WRAP:
            return GL_DECR_WRAP;
        }
    
        return GL_KEEP;
    }
    
    static GLenum translate_stencil_func(SceGxmStencilFunc stencil_func) {
        R_PROFILE(__func__);
    
        switch (stencil_func) {
        case SCE_GXM_STENCIL_FUNC_NEVER:
            return GL_NEVER;
        case SCE_GXM_STENCIL_FUNC_LESS:
            return GL_LESS;
        case SCE_GXM_STENCIL_FUNC_EQUAL:
            return GL_EQUAL;
        case SCE_GXM_STENCIL_FUNC_LESS_EQUAL:
            return GL_LEQUAL;
        case SCE_GXM_STENCIL_FUNC_GREATER:
            return GL_GREATER;
        case SCE_GXM_STENCIL_FUNC_NOT_EQUAL:
            return GL_NOTEQUAL;
        case SCE_GXM_STENCIL_FUNC_GREATER_EQUAL:
            return GL_GEQUAL;
        case SCE_GXM_STENCIL_FUNC_ALWAYS:
            return GL_ALWAYS;
        }
    
        return GL_ALWAYS;
    }
    
    static void apply_stencil_state(GLenum face, const GxmStencilState &state) {
        glStencilOpSeparate(face,
                            translate_stencil_op(state.stencil_fail),
                            translate_stencil_op(state.depth_fail),
                            translate_stencil_op(state.depth_pass));
        glStencilFuncSeparate(face, translate_stencil_func(state.func), state.ref, state.compare_mask);
        glStencilMaskSeparate(face, state.write_mask);
    }
    
    static void apply_state(const GxmContextState &state) {
        R_PROFILE(__func__);
        
        // Stencil.
        if (state.depth_stencil_surface.stencilData) {
            glEnable(GL_STENCIL_TEST);
            apply_stencil_state(GL_FRONT, state.front_stencil);
            apply_stencil_state(GL_BACK, state.back_stencil);
        } else {
            glDisable(GL_STENCIL_TEST);
        }
    }
    
    static size_t attribute_format_size(SceGxmAttributeFormat format) {
        R_PROFILE(__func__);
    
        switch (format) {
        case SCE_GXM_ATTRIBUTE_FORMAT_U8:
        case SCE_GXM_ATTRIBUTE_FORMAT_U8N:
        case SCE_GXM_ATTRIBUTE_FORMAT_S8:
        case SCE_GXM_ATTRIBUTE_FORMAT_S8N:
            return 1;
        case SCE_GXM_ATTRIBUTE_FORMAT_U16:
        case SCE_GXM_ATTRIBUTE_FORMAT_U16N:
        case SCE_GXM_ATTRIBUTE_FORMAT_S16:
        case SCE_GXM_ATTRIBUTE_FORMAT_S16N:
        case SCE_GXM_ATTRIBUTE_FORMAT_F16:
            return 2;
        case SCE_GXM_ATTRIBUTE_FORMAT_F32:
            return 4;
        default:
            LOG_ERROR("Unsupported attribute format {}", log_hex(format));
            return 4;
        }
    }
    
    static GLenum attribute_format_to_gl_type(SceGxmAttributeFormat format) {
        R_PROFILE(__func__);
    
        switch (format) {
        case SCE_GXM_ATTRIBUTE_FORMAT_U8:
        case SCE_GXM_ATTRIBUTE_FORMAT_U8N:
            return GL_UNSIGNED_BYTE;
        case SCE_GXM_ATTRIBUTE_FORMAT_S8:
        case SCE_GXM_ATTRIBUTE_FORMAT_S8N:
            return GL_BYTE;
        case SCE_GXM_ATTRIBUTE_FORMAT_U16:
        case SCE_GXM_ATTRIBUTE_FORMAT_U16N:
            return GL_UNSIGNED_SHORT;
        case SCE_GXM_ATTRIBUTE_FORMAT_S16:
        case SCE_GXM_ATTRIBUTE_FORMAT_S16N:
            return GL_SHORT;
        case SCE_GXM_ATTRIBUTE_FORMAT_F16:
            return GL_HALF_FLOAT;
        case SCE_GXM_ATTRIBUTE_FORMAT_F32:
            return GL_FLOAT;
        case SCE_GXM_ATTRIBUTE_FORMAT_UNTYPED:
            LOG_WARN("Unsupported attribute format SCE_GXM_ATTRIBUTE_FORMAT_UNTYPED");
            return GL_UNSIGNED_BYTE;
        }
        
        LOG_ERROR("Unsupported attribute format {}", log_hex(format));
        return GL_UNSIGNED_BYTE;
    }
    
    static GLboolean attribute_format_normalised(SceGxmAttributeFormat format) {
        R_PROFILE(__func__);
    
        switch (format) {
        case SCE_GXM_ATTRIBUTE_FORMAT_U8N:
        case SCE_GXM_ATTRIBUTE_FORMAT_S8N:
        case SCE_GXM_ATTRIBUTE_FORMAT_U16N:
        case SCE_GXM_ATTRIBUTE_FORMAT_S16N:
            return GL_TRUE;
        default:
            return GL_FALSE;
        }
    }
    
    static GLenum translate_primitive(SceGxmPrimitiveType primType) {
        R_PROFILE(__func__);
    
        switch (primType) {
        case SCE_GXM_PRIMITIVE_TRIANGLES:
            return GL_TRIANGLES;
        case SCE_GXM_PRIMITIVE_TRIANGLE_STRIP:
            return GL_TRIANGLE_STRIP;
        case SCE_GXM_PRIMITIVE_TRIANGLE_FAN:
            return GL_TRIANGLE_FAN;
        case SCE_GXM_PRIMITIVE_LINES:
            return GL_LINES;
        case SCE_GXM_PRIMITIVE_POINTS:
            return GL_POINTS;
        case SCE_GXM_PRIMITIVE_TRIANGLE_EDGES: // Todo: Implement this
            LOG_WARN("Unsupported primitive: SCE_GXM_PRIMITIVE_TRIANGLE_EDGES, using GL_TRIANGLES instead");
            return GL_TRIANGLES;
        }
        return GL_TRIANGLES;
    }
    
    bool create(Context &context, SDL_Window *window) {
        R_PROFILE(__func__);
        
        assert(SDL_GL_GetCurrentContext() == nullptr);
        context.gl = GLContextPtr(SDL_GL_CreateContext(window), SDL_GL_DeleteContext);
        assert(context.gl != nullptr);
        
        const glbinding::GetProcAddress get_proc_address = [](const char *name) {
            return reinterpret_cast<ProcAddress>(SDL_GL_GetProcAddress(name));
        };
        Binding::initialize(get_proc_address, false);
        
        LOG_INFO("GL_VERSION = {}", glGetString(GL_VERSION));
        LOG_INFO("GL_SHADING_LANGUAGE_VERSION = {}", glGetString(GL_SHADING_LANGUAGE_VERSION));
        
//        set_viewport(ctx->state.viewport, host.display.image_size.width, host.display.image_size.height);
        
        // TODO This is just for debugging.
        glClearColor(0.0625f, 0.125f, 0.25f, 0);
        
        if (!init(context.texture_cache) || !context.vertex_array.init(glGenVertexArrays, glDeleteVertexArrays) || !context.element_buffer.init(glGenBuffers, glDeleteBuffers) || !context.stream_vertex_buffers.init(glGenBuffers, glDeleteBuffers)) {
            return false;
        }
        
        glBindVertexArray(context.vertex_array[0]);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, context.element_buffer[0]);
        
        return true;
    }
    
    bool create(RenderTarget &rt, const SceGxmRenderTargetParams &params) {
        R_PROFILE(__func__);
        
        if (!rt.renderbuffers.init(glGenRenderbuffers, glDeleteRenderbuffers) || !rt.framebuffer.init(glGenFramebuffers, glDeleteFramebuffers)) {
            return false;
        }
        
        glBindRenderbuffer(GL_RENDERBUFFER, rt.renderbuffers[0]);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA, params.width, params.height);
        glBindRenderbuffer(GL_RENDERBUFFER, rt.renderbuffers[1]);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, params.width, params.height);
        glBindRenderbuffer(GL_RENDERBUFFER, 0);
        
        glBindFramebuffer(GL_FRAMEBUFFER, rt.framebuffer[0]);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, rt.renderbuffers[0]);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rt.renderbuffers[1]);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        
        return true;
    }
    
    void begin_scene(const RenderTarget &rt) {
        R_PROFILE(__func__);
        
        glBindFramebuffer(GL_FRAMEBUFFER, rt.framebuffer[0]);
        
        // TODO This is just for debugging.
        glClear(GL_COLOR_BUFFER_BIT);
    }
    
    void end_scene(Context &context, size_t width, size_t height, size_t stride_in_pixels, uint32_t *pixels) {
        R_PROFILE(__func__);
        
        glPixelStorei(GL_PACK_ROW_LENGTH, stride_in_pixels);
        glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
        glPixelStorei(GL_PACK_ROW_LENGTH, 0);
        
        flip_vertically(pixels, width, height, stride_in_pixels);
        
        ++context.texture_cache.timestamp;
        
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }
    
    bool draw(Context &context, const GxmContextState &state, SceGxmPrimitiveType type, SceGxmIndexFormat format, const void *indices, size_t count, const MemState &mem) {
        R_PROFILE(__func__);
        
        // TODO Use some kind of caching to avoid setting every draw call?
        const SharedGLObject program = get_program(context, state, mem);
        if (!program) {
            return false;
        }
        glUseProgram(program->get());
        
        // TODO Use some kind of caching to avoid setting every draw call?
        set_uniforms(program->get(), context, state, mem);
        
        apply_state(state);
        
        // Upload index data.
        const GLsizeiptr index_size = (format == SCE_GXM_INDEX_FORMAT_U16) ? 2 : 4;
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, index_size * count, indices, GL_STREAM_DRAW);
        
        // Compute size of vertex data.
        size_t max_index = 0;
        if (format == SCE_GXM_INDEX_FORMAT_U16) {
            const uint16_t *const data = static_cast<const uint16_t *>(indices);
            max_index = *std::max_element(&data[0], &data[count]);
        } else {
            const uint32_t *const data = static_cast<const uint32_t *>(indices);
            max_index = *std::max_element(&data[0], &data[count]);
        }
        const SceGxmVertexProgram &vertex_program = *state.vertex_program.get(mem);
        size_t max_data_length[SCE_GXM_MAX_VERTEX_STREAMS] = {};
        for (const emu::SceGxmVertexAttribute &attribute : vertex_program.attributes) {
            const SceGxmAttributeFormat attribute_format = static_cast<SceGxmAttributeFormat>(attribute.format);
            const size_t attribute_size = attribute_format_size(attribute_format) * attribute.componentCount;
            const SceGxmVertexStream &stream = vertex_program.streams[attribute.streamIndex];
            const size_t data_length = attribute.offset + (max_index * stream.stride) + attribute_size;
            max_data_length[attribute.streamIndex] = std::max(max_data_length[attribute.streamIndex], data_length);
        }
        
        // Upload vertex data.
        for (size_t stream_index = 0; stream_index < SCE_GXM_MAX_VERTEX_STREAMS; ++stream_index) {
            const size_t data_length = max_data_length[stream_index];
            const void *const data = state.stream_data[stream_index].get(mem);
            glBindBuffer(GL_ARRAY_BUFFER, context.stream_vertex_buffers[stream_index]);
            glBufferData(GL_ARRAY_BUFFER, data_length, data, GL_STREAM_DRAW);
        }
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        
        // Set vertex attribute parameters.
        // TODO Only set attrib pointer when changed.
        // TODO Move vertex array object to program cache?
        for (const emu::SceGxmVertexAttribute &attribute : vertex_program.attributes) {
            const SceGxmVertexStream &stream = vertex_program.streams[attribute.streamIndex];
            const SceGxmAttributeFormat attribute_format = static_cast<SceGxmAttributeFormat>(attribute.format);
            const GLenum type = attribute_format_to_gl_type(attribute_format);
            const GLboolean normalised = attribute_format_normalised(attribute_format);
            const int attrib_location = attribute.regIndex / sizeof(uint32_t);
            
            glBindBuffer(GL_ARRAY_BUFFER, context.stream_vertex_buffers[attribute.streamIndex]);
            glVertexAttribPointer(attrib_location, attribute.componentCount, type, normalised, stream.stride, reinterpret_cast<const GLvoid *>(attribute.offset));
            glEnableVertexAttribArray(attrib_location);
        }
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        
        const GLenum mode = translate_primitive(type);
        const GLenum gl_type = format == SCE_GXM_INDEX_FORMAT_U16 ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT;
        glDrawElements(mode, count, gl_type, nullptr);
        
        // Disable vertex attribute arrays.
        for (const emu::SceGxmVertexAttribute &attribute : vertex_program.attributes) {
            const int attrib_location = attribute.regIndex / sizeof(uint32_t);
            glDisableVertexAttribArray(attrib_location);
        }
        
        return true;
    }
}
