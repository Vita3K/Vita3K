#include <renderer/functions.h>

#include "functions.h"
#include "profile.h"

#include <renderer/types.h>

#include <gxm/types.h>
#include <util/log.h>

namespace renderer {
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

void draw(Context &context, const GxmContextState &state, SceGxmPrimitiveType type, SceGxmIndexFormat format, const void *indices, size_t count, const MemState &mem) {
    R_PROFILE(__func__);

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
        glBufferData(GL_ARRAY_BUFFER, data_length, data, GL_STREAM_DRAW);
    }
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    // Draw.
    const GLenum mode = translate_primitive(type);
    const GLenum gl_type = format == SCE_GXM_INDEX_FORMAT_U16 ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT;
    glDrawElements(mode, static_cast<gl::GLsizei>(count), gl_type, nullptr);
}
} // namespace renderer
