// Vita3K emulator project
// Copyright (C) 2025 Vita3K team
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
#include <util/align.h>
#include <util/log.h>
#include <util/vector_utils.h>

#include <shader/spirv_recompiler.h>

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

void sync_mask(const GLState &state, GLContext &context, const MemState &mem) {
    GLubyte initial_byte = context.record.depth_stencil_surface.mask ? 0xFF : 0;

#ifdef __ANDROID__
    auto width = context.render_target->width;
    auto height = context.render_target->height;
    std::vector<GLubyte> emptyData(width * height * 4, initial_byte);
    GLint texId;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &texId);
    glBindTexture(GL_TEXTURE_2D, context.render_target->masktexture[0]);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, &emptyData[0]);
    glBindTexture(GL_TEXTURE_2D, texId);
#else
    GLubyte clear_bytes[4] = { initial_byte, initial_byte, initial_byte, initial_byte };
    glClearTexImage(context.render_target->masktexture[0], 0, GL_RGBA, GL_UNSIGNED_BYTE, clear_bytes);
#endif
}

void sync_viewport_flat(const GLState &state, GLContext &context) {
    const GLsizei display_w = context.record.color_surface.width;
    const GLsizei display_h = context.record.color_surface.height;

    glViewport(0, static_cast<GLint>((context.current_framebuffer_height - display_h) * state.res_multiplier),
        static_cast<GLsizei>(display_w * state.res_multiplier), static_cast<GLsizei>(display_h * state.res_multiplier));
    glDepthRange(0, 1);
}

void sync_viewport_real(const GLState &state, GLContext &context, const float xOffset, const float yOffset, const float zOffset,
    const float xScale, const float yScale, const float zScale) {
    const GLfloat ymin = yOffset + yScale;
    const GLfloat ymax = yOffset - yScale - 1;

    const GLfloat w = std::abs(2 * xScale);
    const GLfloat h = std::abs(2 * yScale);
    const GLfloat x = xOffset - std::abs(xScale);
    const GLfloat y = std::min<GLfloat>(ymin, ymax);

    glViewport(x * state.res_multiplier, y * state.res_multiplier, w * state.res_multiplier, h * state.res_multiplier);
    glDepthRangef(0.0f, 1.0f);
}

void sync_clipping(const GLState &state, GLContext &context) {
    const GLsizei display_h = context.current_framebuffer_height;
    const GLsizei scissor_x = context.record.region_clip_min.x;
    GLsizei scissor_y = 0;

    if (context.record.viewport_flip[1] == -1.0f)
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
        glScissor(static_cast<GLint>(scissor_x * state.res_multiplier), static_cast<GLint>(scissor_y * state.res_multiplier),
            static_cast<GLsizei>(scissor_w * state.res_multiplier), static_cast<GLsizei>(scissor_h * state.res_multiplier));
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

void sync_depth_data(const renderer::GxmRecordState &state) {
    // Depth test.
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);

    if (!state.depth_stencil_surface.force_load && state.depth_stencil_surface.depth_data) {
        glClearDepthf(state.depth_stencil_surface.background_depth);
        glClear(GL_DEPTH_BUFFER_BIT);
    }
}

void sync_stencil_func(const GxmStencilStateOp &state_op, const GxmStencilStateValues &state_vals, const MemState &mem, const bool is_back_stencil) {
    const GLenum face = is_back_stencil ? GL_BACK : GL_FRONT;

    glStencilOpSeparate(face,
        translate_stencil_op(state_op.stencil_fail),
        translate_stencil_op(state_op.depth_fail),
        translate_stencil_op(state_op.depth_pass));
    glStencilFuncSeparate(face, translate_stencil_func(state_op.func), state_vals.ref, state_vals.compare_mask);
    glStencilMaskSeparate(face, state_vals.write_mask);
}

void sync_stencil_data(const GxmRecordState &state, const MemState &mem) {
    // Stencil test.
    glEnable(GL_STENCIL_TEST);
    glStencilMask(GL_TRUE);
    if (!state.depth_stencil_surface.force_load) {
        glClearStencil(state.depth_stencil_surface.stencil);
        glClear(GL_STENCIL_BUFFER_BIT);
    }
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

void sync_point_line_width(const GLState &state, const std::uint32_t width, const bool is_front) {
    // Point Line Width
    if (is_front) {
        glLineWidth(width * state.res_multiplier);
        glPointSize(width * state.res_multiplier);
    }
}

void sync_depth_bias(const int factor, const int unit, const bool is_front) {
    // Depth Bias
    if (is_front) {
        glPolygonOffset(static_cast<GLfloat>(factor), static_cast<GLfloat>(unit));
    }
}

void sync_texture(GLState &state, GLContext &context, MemState &mem, std::size_t index, SceGxmTexture texture,
    const Config &config) {
    Address data_addr = texture.data_addr << 2;

    if (!is_valid_addr(mem, data_addr)) {
        LOG_WARN("Texture has freed data.");
        return;
    }

    const SceGxmTextureFormat format = gxm::get_format(texture);
    const SceGxmTextureBaseFormat base_format = gxm::get_base_format(format);
    if (gxm::is_paletted_format(base_format) && texture.palette_addr == 0) {
        LOG_WARN("Ignoring null palette texture");
        return;
    }

    if (index >= SCE_GXM_MAX_TEXTURE_UNITS) {
        // Vertex textures
        context.shader_hints.vertex_textures[index - SCE_GXM_MAX_TEXTURE_UNITS] = format;
    } else {
        context.shader_hints.fragment_textures[index] = format;
    }

    glActiveTexture(static_cast<GLenum>(static_cast<std::size_t>(GL_TEXTURE0) + index));

    std::uint64_t texture_as_surface = 0;
    const GLint *swizzle_surface = nullptr;
    bool only_nearest = false;

    if (context.record.color_surface.data.address() == data_addr) {
        texture_as_surface = context.current_color_attachment;
        swizzle_surface = color::translate_swizzle(context.record.color_surface.colorFormat);

        vector_utils::push_if_not_exists(context.self_sampling_indices, index);
    } else {
        vector_utils::erase_first(context.self_sampling_indices, index);

        SceGxmColorBaseFormat format_target_of_texture;

        std::uint16_t width = static_cast<std::uint16_t>(gxm::get_width(texture));
        std::uint16_t height = static_cast<std::uint16_t>(gxm::get_height(texture));

        if (renderer::texture::convert_base_texture_format_to_base_color_format(base_format, format_target_of_texture)) {
            std::uint16_t stride_in_pixels = width;

            switch (texture.texture_type()) {
            case SCE_GXM_TEXTURE_LINEAR_STRIDED:
                stride_in_pixels = static_cast<std::uint16_t>(gxm::get_stride_in_bytes(texture)) / ((gxm::bits_per_pixel(base_format) + 7) >> 3);
                break;
            case SCE_GXM_TEXTURE_LINEAR:
                // when the texture is linear, the stride should be aligned to 8 pixels
                stride_in_pixels = align(stride_in_pixels, 8);
                break;
            case SCE_GXM_TEXTURE_TILED:
                // tiles are 32x32
                stride_in_pixels = align(stride_in_pixels, 32);
                break;
            }

            std::uint32_t swizz_raw = 0;

            texture_as_surface = state.surface_cache.retrieve_color_surface_texture_handle(
                state, width, height, stride_in_pixels, format_target_of_texture, Ptr<void>(data_addr),
                SurfaceTextureRetrievePurpose::READING, swizz_raw);

            swizzle_surface = color::translate_swizzle(static_cast<SceGxmColorFormat>(format_target_of_texture | swizz_raw));
            only_nearest = color::is_write_surface_non_linearity_filtering(format_target_of_texture);
        }

        // Try to retrieve S24D8 texture
        if (!texture_as_surface) {
            SceGxmDepthStencilSurface lookup_temp;
            lookup_temp.depth_data = data_addr;
            lookup_temp.stencil_data.reset();

            texture_as_surface = state.surface_cache.retrieve_depth_stencil_texture_handle(state, mem, lookup_temp, width, height, true);
            if (texture_as_surface) {
                only_nearest = true;
            }
        }
    }

    if (texture_as_surface != 0) {
        glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(texture_as_surface));

        if (only_nearest) {
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        }

        if (base_format != SCE_GXM_TEXTURE_BASE_FORMAT_X8U24) {
            const GLint *swizzle = texture::translate_swizzle(format);

            if (swizzle) {
                if (swizzle_surface) {
                    if (std::memcmp(swizzle_surface, swizzle, 16) != 0) {
                        // Surface is stored in RGBA in GPU memory, unless in other circumstances. So we must reverse order
                        for (int i = 0; i < 4; i++) {
                            if ((swizzle[i] < GL_RED) || (swizzle[i] > GL_ALPHA)) {
                                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_R + i, swizzle[i]);
                            } else {
                                for (int j = 0; j < 4; j++) {
                                    if (swizzle[i] == swizzle_surface[j]) {
                                        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_R + i, GL_RED + j);
                                        break;
                                    }
                                }
                            }
                        }
                    } else {
                        const GLint default_rgba[4] = { GL_RED, GL_GREEN, GL_BLUE, GL_ALPHA };
#ifdef __ANDROID__
                        for (int i = 0; i < 4; i++) {
                            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_R + i, default_rgba[i]);
                        }
#else
                        glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_RGBA, default_rgba);
#endif
                    }
                } else {
                    static bool has_happened = false;
                    LOG_TRACE_IF(!has_happened, "No surface swizzle found, use default texture swizzle");
                    has_happened = true;
#ifdef __ANDROID__
                    for (int i = 0; i < 4; i++) {
                        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_R + i, swizzle[i]);
                    }
#else
                    glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_RGBA, swizzle);
#endif
                }
            }
        }
    } else {
        if (config.texture_cache) {
            state.texture_cache.cache_and_bind_texture(texture, mem);
        } else {
            texture::bind_texture_without_cache(state.texture_cache, texture, mem);
        }
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

    // Each draw will upload the stream data. Assuming that, we can just bind buffer, upload data
    // The GXM submit side should already submit used buffer, but we just delete all just in case
    std::array<std::size_t, SCE_GXM_MAX_VERTEX_STREAMS> offset_in_buffer;
    for (std::size_t i = 0; i < SCE_GXM_MAX_VERTEX_STREAMS; i++) {
        if (state.vertex_streams[i].data) {
            std::pair<std::uint8_t *, std::size_t> result = context.vertex_stream_ring_buffer.allocate(state.vertex_streams[i].size);
            if (!result.first) {
                LOG_ERROR("Failed to allocate vertex stream data from GPU!");
            } else {
                std::memcpy(result.first, state.vertex_streams[i].data.get(mem), state.vertex_streams[i].size);
                offset_in_buffer[i] = result.second;
            }

            state.vertex_streams[i].data = nullptr;
            state.vertex_streams[i].size = 0;
        } else {
            offset_in_buffer[i] = 0;
        }
    }

    glBindBuffer(GL_ARRAY_BUFFER, context.vertex_stream_ring_buffer.handle());

    for (const SceGxmVertexAttribute &attribute : vertex_program.attributes) {
        if (!glvert->attribute_infos.contains(attribute.regIndex))
            continue;

        const SceGxmVertexStream &stream = vertex_program.streams[attribute.streamIndex];

        GLenum type = attribute_format_to_gl_type(attribute.format);
        const GLboolean normalized = attribute_format_normalized(attribute.format);

        bool upload_integral = false;

        shader::usse::AttributeInformation info = glvert->attribute_infos.at(attribute.regIndex);
        int attrib_location = info.location;

        // these 2 values are only used when a matrix is used as a vertex attribute
        // this is only supported for regformated attribute for now
        uint32_t array_size = 1;
        uint32_t array_element_size = 0;
        uint8_t component_count = attribute.componentCount;

        if (info.regformat) {
            component_count = info.component_count;
            switch (info.gxm_type) {
            case SCE_GXM_PARAMETER_TYPE_U8:
            case SCE_GXM_PARAMETER_TYPE_S8:
            case SCE_GXM_PARAMETER_TYPE_C10:
                type = GL_UNSIGNED_BYTE;
                break;
            case SCE_GXM_PARAMETER_TYPE_U16:
            case SCE_GXM_PARAMETER_TYPE_S16:
            case SCE_GXM_PARAMETER_TYPE_F16:
                type = GL_UNSIGNED_SHORT;
                break;
            default:
                // U32 format
                type = GL_UNSIGNED_INT;
                break;
            }
            upload_integral = true;

            if (info.gxm_type == SCE_GXM_PARAMETER_TYPE_C10)
                // this is 10-bit and not 8-bit
                component_count = (component_count * 10 + 7) / 8;

            if (component_count > 4) {
                // a matrix is used as an attribute, pack everything into an array of vec4
                array_size = (component_count + 3) / 4;
                array_element_size = 4 * gxm::attribute_format_size(attribute.format);
                component_count = 4;
            }
        } else {
            switch (info.gxm_type) {
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
        }

        const std::uint16_t stream_index = attribute.streamIndex;

        for (uint32_t i = 0; i < array_size; i++) {
            if (upload_integral || (attribute.format == SCE_GXM_ATTRIBUTE_FORMAT_UNTYPED)) {
                glVertexAttribIPointer(attrib_location + i, component_count, type, stream.stride, reinterpret_cast<const GLvoid *>(i * array_element_size + attribute.offset + offset_in_buffer[stream_index]));
            } else {
                glVertexAttribPointer(attrib_location + i, component_count, type, normalized, stream.stride, reinterpret_cast<const GLvoid *>(i * array_element_size + attribute.offset + offset_in_buffer[stream_index]));
            }

            glEnableVertexAttribArray(attrib_location + i);

            if (gxm::is_stream_instancing(static_cast<SceGxmIndexSource>(stream.indexSource))) {
                glVertexAttribDivisor(attrib_location + i, 1);
            } else {
                glVertexAttribDivisor(attrib_location + i, 0);
            }
        }
    }
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}
} // namespace renderer::gl
