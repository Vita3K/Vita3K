// Vita3K emulator project
// Copyright (C) 2021 Vita3K team
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License along
// with this program; if not, write to the Free Software Foundation, Inc.,
// 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

#include <renderer/functions.h>
#include <renderer/profile.h>
#include <renderer/types.h>

#include <renderer/gl/functions.h>
#include <renderer/gl/state.h>
#include <renderer/gl/types.h>

#include <gxm/functions.h>
#include <gxm/types.h>
#include <util/log.h>

#include <cmath>

namespace renderer::gl {

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

void sync_mask(GLContext &context, const MemState &mem) {
    auto control = context.record.depth_stencil_surface.control.get(mem);
    auto width = context.render_target->width;
    auto height = context.render_target->height;
    GLubyte initial_byte;
    if (control) {
        initial_byte = control->backgroundMask ? 0xFF : 0;
    } else {
        // always accept
        initial_byte = 0xFF;
    }
    std::vector<GLubyte> emptyData(width * height * 4, initial_byte);
    GLint texId;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &texId);
    glBindTexture(GL_TEXTURE_2D, context.render_target->masktexture[0]);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, &emptyData[0]);
    glBindTexture(GL_TEXTURE_2D, texId);
}

void sync_viewport_flat(GLContext &context) {
    const GLsizei display_w = context.record.color_surface.width;
    const GLsizei display_h = context.record.color_surface.height;
    const float previous_flip_y = context.viewport_flip[1];

    context.viewport_flip[0] = 1.0f;
    context.viewport_flip[1] = -1.0f;
    context.viewport_flip[2] = 1.0f;
    context.viewport_flip[3] = 1.0f;

    context.record.viewport_flat = true;

    glViewport(0, context.current_framebuffer_height - display_h, display_w, display_h);
    glDepthRange(0, 1);

    if (previous_flip_y != context.viewport_flip[1]) {
        // We need to sync again state that uses the flip
        sync_cull(context.record);
        sync_clipping(context);
    }
}

void sync_viewport_real(GLContext &context, const float xOffset, const float yOffset, const float zOffset,
    const float xScale, const float yScale, const float zScale) {
    const GLfloat ymin = yOffset + yScale;
    const GLfloat ymax = yOffset - yScale - 1;

    const GLfloat w = std::abs(2 * xScale);
    const GLfloat h = std::abs(2 * yScale);
    const GLfloat x = xOffset - std::abs(xScale);
    const GLfloat y = std::min<GLfloat>(ymin, ymax);

    const float previous_flip_y = context.viewport_flip[1];

    context.viewport_flip[0] = 1.0f;
    context.viewport_flip[1] = (ymin < ymax) ? -1.0f : 1.0f;
    context.viewport_flip[2] = 1.0f;
    context.viewport_flip[3] = 1.0f;

    context.record.viewport_flat = false;

    glViewportIndexedf(0, x, y, w, h);
    glDepthRange(zOffset - zScale, zOffset + zScale);

    if (previous_flip_y != context.viewport_flip[1]) {
        // We need to sync again state that uses the flip
        sync_cull(context.record);
        sync_clipping(context);
    }
}

void sync_clipping(GLContext &context) {
    const GLsizei display_h = context.current_framebuffer_height;
    const GLsizei scissor_x = context.record.region_clip_min.x;
    GLsizei scissor_y = 0;

    if (context.viewport_flip[1] == -1.0f)
        scissor_y = context.record.region_clip_min.y;
    else
        scissor_y = display_h - context.record.region_clip_max.y - 1;

    const GLsizei scissor_w = context.record.region_clip_max.x - context.record.region_clip_min.x + 1;
    const GLsizei scissor_h = context.record.region_clip_max.y - context.record.region_clip_min.y + 1;

    switch (context.record.region_clip_mode) {
    case SCE_GXM_REGION_CLIP_NONE:
        glDisable(GL_SCISSOR_TEST);
        break;
    case SCE_GXM_REGION_CLIP_ALL:
        glEnable(GL_SCISSOR_TEST);
        glScissor(0, 0, 0, 0);
        break;
    case SCE_GXM_REGION_CLIP_OUTSIDE:
        glEnable(GL_SCISSOR_TEST);
        glScissor(scissor_x, scissor_y, scissor_w, scissor_h);
        break;
    case SCE_GXM_REGION_CLIP_INSIDE:
        // TODO: Implement SCE_GXM_REGION_CLIP_INSIDE
        glDisable(GL_SCISSOR_TEST);
        LOG_WARN("Unimplemented region clip mode used: SCE_GXM_REGION_CLIP_INSIDE");
        break;
    }
}

void sync_cull(const GxmRecordState &state) {
    // Culling.
    switch (state.cull_mode) {
    case SCE_GXM_CULL_CCW:
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
        break;
    case SCE_GXM_CULL_CW:
        glEnable(GL_CULL_FACE);
        glCullFace(GL_FRONT);
        break;
    case SCE_GXM_CULL_NONE:
        glDisable(GL_CULL_FACE);
        break;
    }
}

void sync_depth_func(const SceGxmDepthFunc func, const bool is_front) {
    if (is_front)
        glDepthFunc(translate_depth_func(func));
}

void sync_depth_write_enable(const SceGxmDepthWriteMode mode, const bool is_front) {
    if (is_front)
        glDepthMask(mode == SCE_GXM_DEPTH_WRITE_ENABLED ? GL_TRUE : GL_FALSE);
}

bool sync_depth_data(const renderer::GxmRecordState &state) {
    // Depth test.
    if (state.depth_stencil_surface.depthData) {
        glEnable(GL_DEPTH_TEST);
        return true;
    }

    glDisable(GL_DEPTH_TEST);
    return false;
}

void sync_stencil_func(const GxmStencilState &state, const MemState &mem, const bool is_back_stencil) {
    const GLenum face = is_back_stencil ? GL_BACK : GL_FRONT;

    glStencilOpSeparate(face,
        translate_stencil_op(state.stencil_fail),
        translate_stencil_op(state.depth_fail),
        translate_stencil_op(state.depth_pass));
    glStencilFuncSeparate(face, translate_stencil_func(state.func), state.ref, state.compare_mask);
    glStencilMaskSeparate(face, state.write_mask);
}

bool sync_stencil_data(const GxmRecordState &state, const MemState &mem) {
    // Stencil.
    if (state.depth_stencil_surface.stencilData) {
        glEnable(GL_STENCIL_TEST);
        glStencilMask(GL_TRUE);
        glClearStencil(state.depth_stencil_surface.control.get(mem)->backgroundStencil);
        glClear(GL_STENCIL_BUFFER_BIT);
        return true;
    }

    glDisable(GL_STENCIL_TEST);
    return false;
}

void sync_polygon_mode(const SceGxmPolygonMode mode, const bool front) {
    // TODO: Why decap this?
    const GLint face = GL_FRONT_AND_BACK;

    // Polygon Mode.
    switch (mode) {
    case SCE_GXM_POLYGON_MODE_POINT_10UV:
    case SCE_GXM_POLYGON_MODE_POINT:
    case SCE_GXM_POLYGON_MODE_POINT_01UV:
    case SCE_GXM_POLYGON_MODE_TRIANGLE_POINT:
        glPolygonMode(face, GL_POINT);
        break;
    case SCE_GXM_POLYGON_MODE_LINE:
    case SCE_GXM_POLYGON_MODE_TRIANGLE_LINE:
        glPolygonMode(face, GL_LINE);
        break;
    case SCE_GXM_POLYGON_MODE_TRIANGLE_FILL:
        glPolygonMode(face, GL_FILL);
        break;
    }
}

void sync_point_line_width(const std::uint32_t width, const bool is_front) {
    // Point Line Width
    if (is_front) {
        glLineWidth(static_cast<GLfloat>(width));
        glPointSize(static_cast<GLfloat>(width));
    }
}

void sync_depth_bias(const int factor, const int unit, const bool is_front) {
    // Depth Bias
    if (is_front) {
        glPolygonOffset(static_cast<GLfloat>(factor), static_cast<GLfloat>(unit));
    }
}

void sync_texture(GLState &state, GLContext &context, MemState &mem, std::size_t index, SceGxmTexture texture,
    const Config &config, const std::string &base_path, const std::string &title_id) {
    Address data_addr = texture.data_addr << 2;

    const size_t texture_size = renderer::texture::texture_size(texture);
    if (!is_valid_addr_range(mem, data_addr, data_addr + texture_size)) {
        LOG_WARN("Texture has freed data.");
        return;
    }

    const SceGxmTextureFormat format = gxm::get_format(&texture);
    const SceGxmTextureBaseFormat base_format = gxm::get_base_format(format);
    if (gxm::is_paletted_format(base_format) && texture.palette_addr == 0) {
        LOG_WARN("Ignoring null palette texture");
        return;
    }

    glActiveTexture(static_cast<GLenum>(static_cast<std::size_t>(GL_TEXTURE0) + index));

    std::uint64_t texture_as_surface = 0;
    if (context.record.color_surface.data.address() == data_addr) {
        texture_as_surface = context.current_color_attachment;

        if (std::find(context.self_sampling_indices.begin(), context.self_sampling_indices.end(),
                static_cast<GLuint>(index))
            == context.self_sampling_indices.end()) {
            context.self_sampling_indices.push_back(static_cast<GLuint>(index));
        }
    } else {
        auto res = std::find(context.self_sampling_indices.begin(), context.self_sampling_indices.end(),
            static_cast<GLuint>(index));
        if (res != context.self_sampling_indices.end()) {
            context.self_sampling_indices.erase(res);
        }

        texture_as_surface = state.surface_cache.retrieve_color_surface_texture_handle(
            static_cast<std::uint16_t>(gxm::get_width(&texture)),
            static_cast<std::uint16_t>(gxm::get_height(&texture)),
            Ptr<void>(data_addr), renderer::SurfaceTextureRetrievePurpose::READING);
    }

    if (texture_as_surface != 0) {
        glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(texture_as_surface));
    } else {
        if (config.texture_cache) {
            renderer::texture::cache_and_bind_texture(state.texture_cache, texture, mem);
        } else {
            texture::bind_texture(state.texture_cache, texture, mem);
        }
    }

    if (config.dump_textures) {
        auto frag_program = context.record.fragment_program.get(mem);
        auto program = frag_program->program.get(mem);
        const auto program_hash = sha256(program, program->size);

        std::string parameter_name;
        const auto parameters = gxp::program_parameters(*program);
        for (uint32_t i = 0; i < program->parameter_count; ++i) {
            const auto parameter = &parameters[i];
            if (parameter->resource_index == index) {
                parameter_name = gxp::parameter_name_raw(*parameter);
                break;
            }
        }
        renderer::gl::texture::dump(texture, mem, parameter_name, base_path, title_id, program_hash);
    }

    glActiveTexture(GL_TEXTURE0);
}

void sync_blending(const GxmRecordState &state, const MemState &mem) {
    // Blending.
    const SceGxmFragmentProgram &gxm_fragment_program = *state.fragment_program.get(mem);
    const GLFragmentProgram &fragment_program = *reinterpret_cast<GLFragmentProgram *>(
        gxm_fragment_program.renderer_data.get());

    glColorMask(fragment_program.color_mask_red, fragment_program.color_mask_green, fragment_program.color_mask_blue, fragment_program.color_mask_alpha);
    if (fragment_program.blend_enabled) {
        glEnable(GL_BLEND);
        glBlendEquationSeparate(fragment_program.color_func, fragment_program.alpha_func);
        glBlendFuncSeparate(fragment_program.color_src, fragment_program.color_dst, fragment_program.alpha_src, fragment_program.alpha_dst);
    } else {
        glDisable(GL_BLEND);
    }
}

void clear_previous_uniform_storage(GLContext &context) {
    context.vertex_uniform_buffer_storage_ptr.first = nullptr;
    context.vertex_uniform_buffer_storage_ptr.second = 0;
    context.fragment_uniform_buffer_storage_ptr.first = nullptr;
    context.fragment_uniform_buffer_storage_ptr.second = 0;
}

void sync_vertex_streams_and_attributes(GLContext &context, GxmRecordState &state, const MemState &mem) {
    // Vertex attributes.
    const SceGxmVertexProgram &vertex_program = *state.vertex_program.get(mem);
    GLVertexProgram *glvert = reinterpret_cast<GLVertexProgram *>(vertex_program.renderer_data.get());

    if (!glvert->stripped_symbols_checked) {
        // Insert some symbols here
        const SceGxmProgram *vertex_program_body = vertex_program.program.get(mem);
        if (vertex_program_body && (vertex_program_body->primary_reg_count != 0)) {
            for (std::size_t i = 0; i < vertex_program.attributes.size(); i++) {
                glvert->attribute_infos.emplace(vertex_program.attributes[i].regIndex, shader::usse::AttributeInformation(static_cast<std::uint16_t>(i), SCE_GXM_PARAMETER_TYPE_F32));
            }
        }

        glvert->stripped_symbols_checked = true;
    }

    // Each draw will upload the stream data. Assuming that, we can just bind buffer, upload data
    // The GXM submit side should already submit used buffer, but we just delete all just in case
    std::array<std::size_t, SCE_GXM_MAX_VERTEX_STREAMS> offset_in_buffer;
    for (std::size_t i = 0; i < SCE_GXM_MAX_VERTEX_STREAMS; i++) {
        if (state.vertex_streams[i].data) {
            std::pair<std::uint8_t *, std::size_t> result = context.vertex_stream_ring_buffer.allocate(state.vertex_streams[i].size);
            if (!result.first) {
                LOG_ERROR("Failed to allocate vertex stream data from GPU!");
            } else {
                std::memcpy(result.first, state.vertex_streams[i].data, state.vertex_streams[i].size);
                offset_in_buffer[i] = result.second;
            }

            delete[] state.vertex_streams[i].data;

            state.vertex_streams[i].data = nullptr;
            state.vertex_streams[i].size = 0;
        } else {
            offset_in_buffer[i] = 0;
        }
    }

    glBindBuffer(GL_ARRAY_BUFFER, context.vertex_stream_ring_buffer.handle());

    for (const SceGxmVertexAttribute &attribute : vertex_program.attributes) {
        const SceGxmVertexStream &stream = vertex_program.streams[attribute.streamIndex];

        const SceGxmAttributeFormat attribute_format = static_cast<SceGxmAttributeFormat>(attribute.format);
        const GLenum type = attribute_format_to_gl_type(attribute_format);
        const GLboolean normalised = attribute_format_normalised(attribute_format);

        int attrib_location = 0;
        bool upload_integral = false;

        if (glvert->attribute_infos.find(attribute.regIndex) != glvert->attribute_infos.end()) {
            shader::usse::AttributeInformation info = glvert->attribute_infos.at(attribute.regIndex);
            attrib_location = info.location();

            switch (info.gxm_type()) {
            case SCE_GXM_PARAMETER_TYPE_U8:
            case SCE_GXM_PARAMETER_TYPE_S8:
            case SCE_GXM_PARAMETER_TYPE_U16:
            case SCE_GXM_PARAMETER_TYPE_S16:
            case SCE_GXM_PARAMETER_TYPE_U32:
            case SCE_GXM_PARAMETER_TYPE_S32:
                upload_integral = true;
                break;
            default:
                break;
            }

            const std::uint16_t stream_index = attribute.streamIndex;

            if (upload_integral || (attribute_format == SCE_GXM_ATTRIBUTE_FORMAT_UNTYPED)) {
                glVertexAttribIPointer(attrib_location, attribute.componentCount, type, stream.stride, reinterpret_cast<const GLvoid *>(attribute.offset + offset_in_buffer[stream_index]));
            } else {
                glVertexAttribPointer(attrib_location, attribute.componentCount, type, normalised, stream.stride, reinterpret_cast<const GLvoid *>(attribute.offset + offset_in_buffer[stream_index]));
            }

            glEnableVertexAttribArray(attrib_location);

            if (gxm::is_stream_instancing(static_cast<SceGxmIndexSource>(stream.indexSource))) {
                glVertexAttribDivisor(attrib_location, 1);
            } else {
                glVertexAttribDivisor(attrib_location, 0);
            }
        }
    }
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}
} // namespace renderer::gl
