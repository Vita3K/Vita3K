#include <renderer/functions.h>

#include "functions.h"
#include "profile.h"
#include "state.h"

#include <renderer/types.h>
#include <renderer/state.h>

#include <gxm/types.h>
#include <util/log.h>

#include <gxm/functions.h>

namespace renderer::gl {
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

void draw(GLState &renderer, GLContext &context, GxmContextState &state, SceGxmPrimitiveType type, SceGxmIndexFormat format, const void *indices, size_t count, const MemState &mem,
    const char *base_path, const char *title_id, const bool log_active_shaders, const bool log_uniforms) {
    R_PROFILE(__func__);

    GLuint program_id = context.last_draw_program;

    // Trying to cache: the last time vs this time shader pair. Does it different somehow?
    // If it's different, we need to switch. Else just stick to it.
    // Pass 1: Check pointer.
    if (state.last_draw_vertex_program.address() != state.vertex_program.address() ||
        state.last_draw_fragment_program.address() != state.fragment_program.address()) {
        // Pass 2: Check content
        if (!state.last_draw_vertex_program || !state.last_draw_fragment_program || 
            (state.last_draw_vertex_program.get(mem)->renderer_data->hash !=
            state.vertex_program.get(mem)->renderer_data->hash) || 
            (state.last_draw_fragment_program.get(mem)->renderer_data->hash !=
            state.fragment_program.get(mem)->renderer_data->hash)) {

            // Need to recompile!
            SharedGLObject program = gl::compile_program(renderer.program_cache, renderer.vertex_shader_cache,
                renderer.fragment_shader_cache, state, mem, base_path, title_id);
            
            if (!program) {
                LOG_ERROR("Fail to get program!");
            }

            // Use it
            program_id = (*program).get();
            context.last_draw_program = program_id;
        } else {
            program_id = context.last_draw_program;
        }
    }

    if (!program_id) {
        assert(false && "Should never happen");
        glGetIntegerv(GL_CURRENT_PROGRAM, reinterpret_cast<GLint*>(&program_id));
    }

    glUseProgram(program_id);

    // We should set uniforms
    set_uniforms(program_id, state, mem, log_uniforms);

    const SceGxmFragmentProgram &gxm_fragment_program = *state.fragment_program.get(mem);
    const FragmentProgram &fragment_program = *gxm_fragment_program.renderer_data.get();

    // Upload index data.
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, context.element_buffer[0]);
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
        max_data_length[attribute.streamIndex] = std::max<size_t>(max_data_length[attribute.streamIndex], data_length);
    }

    // Upload vertex data.
    for (size_t stream_index = 0; stream_index < SCE_GXM_MAX_VERTEX_STREAMS; ++stream_index) {
        const size_t data_length = max_data_length[stream_index];
        const void *const data = state.stream_data[stream_index].get(mem);
        glBindBuffer(GL_ARRAY_BUFFER, context.stream_vertex_buffers[stream_index]);

        // Orphan the buffer, so we don't have to stall the pipeline, wait for last draw call to finish
        // See OpenGL Insight at 28.3.2. Intel driver likes glBufferData more, so use glBufferData. 
        glBufferData(GL_ARRAY_BUFFER, data_length, nullptr, GL_STREAM_DRAW);
        glBufferData(GL_ARRAY_BUFFER, data_length, data, GL_STREAM_DRAW);
    }
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    // Draw.
    const GLenum mode = translate_primitive(type);
    const GLenum gl_type = format == SCE_GXM_INDEX_FORMAT_U16 ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT;
    glDrawElements(mode, static_cast<GLsizei>(count), gl_type, nullptr);
    
    state.last_draw_vertex_program = state.vertex_program;
    state.last_draw_fragment_program = state.fragment_program;
}
} // namespace renderer
