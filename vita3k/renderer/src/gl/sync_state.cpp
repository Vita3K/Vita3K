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

static void set_stencil_state(GLenum face, const GxmStencilState &state) {
    glStencilOpSeparate(face,
        translate_stencil_op(state.stencil_fail),
        translate_stencil_op(state.depth_fail),
        translate_stencil_op(state.depth_pass));
    glStencilFuncSeparate(face, translate_stencil_func(state.func), state.ref, state.compare_mask);
    glStencilMaskSeparate(face, state.write_mask);
}

void sync_mask(GLContext &context, const GxmContextState &state, const MemState &mem) {
    auto control = state.depth_stencil_surface.control.get(mem);
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

void sync_viewport(GLContext &context, const GxmContextState &state) {
    // Viewport.
    const GLsizei display_w = state.color_surface.width;
    const GLsizei display_h = state.color_surface.height;
    const GxmViewport &viewport = state.viewport;
    const float previous_flip_y = context.viewport_flip[1];

    if (viewport.enable == SCE_GXM_VIEWPORT_ENABLED) {
        const GLfloat ymin = viewport.offset.y + viewport.scale.y;
        const GLfloat ymax = viewport.offset.y - viewport.scale.y - 1;

        const GLfloat w = std::abs(2 * viewport.scale.x);
        const GLfloat h = std::abs(2 * viewport.scale.y);
        const GLfloat x = viewport.offset.x - std::abs(viewport.scale.x);
        const GLfloat y = std::min<GLfloat>(ymin, ymax);

        context.viewport_flip[0] = 1.0f;
        context.viewport_flip[1] = (ymin < ymax) ? -1.0f : 1.0f;
        context.viewport_flip[2] = 1.0f;
        context.viewport_flip[3] = 1.0f;

        glViewportIndexedf(0, x, y, w, h);
        glDepthRange(viewport.offset.z - viewport.scale.z, viewport.offset.z + viewport.scale.z);
    } else {
        context.viewport_flip[0] = 1.0f;
        context.viewport_flip[1] = -1.0f;
        context.viewport_flip[2] = 1.0f;
        context.viewport_flip[3] = 1.0f;

        glViewport(0, 0, display_w, display_h);
        glDepthRange(0, 1);
    }

    if (previous_flip_y != context.viewport_flip[1]) {
        // We need to sync again state that uses the flip
        sync_cull(context, state);
        sync_clipping(context, state);
    }
}

void sync_clipping(GLContext &context, const GxmContextState &state) {
    const GLsizei display_h = state.color_surface.height;
    const GLsizei scissor_x = state.region_clip_min.x;
    GLsizei scissor_y = 0;

    if (context.viewport_flip[1] == -1.0f)
        scissor_y = state.region_clip_min.y;
    else
        scissor_y = display_h - state.region_clip_max.y - 1;

    const GLsizei scissor_w = state.region_clip_max.x - state.region_clip_min.x + 1;
    const GLsizei scissor_h = state.region_clip_max.y - state.region_clip_min.y + 1;
    switch (state.region_clip_mode) {
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

void sync_cull(GLContext &context, const GxmContextState &state) {
    // Culling.
    switch (state.cull_mode) {
    case SCE_GXM_CULL_CCW:
        glEnable(GL_CULL_FACE);
        glCullFace(context.viewport_flip[1] == 1.0f ? GL_FRONT : GL_BACK);
        break;
    case SCE_GXM_CULL_CW:
        glEnable(GL_CULL_FACE);
        glCullFace(context.viewport_flip[1] == 1.0f ? GL_BACK : GL_FRONT);
        break;
    case SCE_GXM_CULL_NONE:
        glDisable(GL_CULL_FACE);
        break;
    }
}

void sync_front_depth_func(const GxmContextState &state) {
    glDepthFunc(translate_depth_func(state.front_depth_func));
}

void sync_front_depth_write_enable(const GxmContextState &state) {
    glDepthMask(state.front_depth_write_enable == SCE_GXM_DEPTH_WRITE_ENABLED ? GL_TRUE : GL_FALSE);
}

bool sync_depth_data(const GxmContextState &state) {
    // Depth test.
    if (state.depth_stencil_surface.depthData) {
        glEnable(GL_DEPTH_TEST);
        return true;
    }

    glDisable(GL_DEPTH_TEST);
    return false;
}

void sync_stencil_func(const GxmContextState &state, const MemState &mem, const bool is_back_stencil) {
    set_stencil_state(is_back_stencil ? GL_BACK : GL_FRONT, is_back_stencil ? state.back_stencil : state.front_stencil);
}

bool sync_stencil_data(const GxmContextState &state, const MemState &mem) {
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

void sync_front_polygon_mode(const GxmContextState &state) {
    // Polygon Mode.
    switch (state.front_polygon_mode) {
    case SCE_GXM_POLYGON_MODE_POINT_10UV:
    case SCE_GXM_POLYGON_MODE_POINT:
    case SCE_GXM_POLYGON_MODE_POINT_01UV:
    case SCE_GXM_POLYGON_MODE_TRIANGLE_POINT:
        glPolygonMode(GL_FRONT_AND_BACK, GL_POINT);
        break;
    case SCE_GXM_POLYGON_MODE_LINE:
    case SCE_GXM_POLYGON_MODE_TRIANGLE_LINE:
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        break;
    case SCE_GXM_POLYGON_MODE_TRIANGLE_FILL:
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        break;
    }
}

void sync_front_point_line_width(const GxmContextState &state) {
    // Point Line Width
    glLineWidth(static_cast<GLfloat>(state.front_point_line_width));
    glPointSize(static_cast<GLfloat>(state.front_point_line_width));
}

void sync_front_depth_bias(const GxmContextState &state) {
    // Depth Bias
    glPolygonOffset(static_cast<GLfloat>(state.front_depth_bias_factor), static_cast<GLfloat>(state.front_depth_bias_units));
}

void sync_texture(GLContext &context, const GxmContextState &state, const MemState &mem, std::size_t index,
    const Config &config, const std::string &base_path, const std::string &title_id) {
    const SceGxmTexture &texture = state.fragment_textures[index];

    if (texture.data_addr == 0) {
        LOG_WARN("Texture has null data.");
        return;
    }

    const SceGxmTextureFormat format = gxm::get_format(&texture);
    if (gxm::is_paletted_format(format) && texture.palette_addr == 0) {
        LOG_WARN("Ignoring null palette texture");
        return;
    }

    glActiveTexture(static_cast<GLenum>(static_cast<std::size_t>(GL_TEXTURE0) + index));

    if (config.texture_cache) {
        renderer::texture::cache_and_bind_texture(context.texture_cache, texture, mem);
    } else {
        texture::bind_texture(context.texture_cache, texture, mem);
    }

    if (config.dump_textures) {
        auto frag_program = state.fragment_program.get(mem);
        auto program = frag_program->program.get(mem);
        const auto program_hash = sha256(program, program->size);

        std::string parameter_name;
        const auto fragment_program = state.fragment_program.get(mem);
        const auto fragment_program_gxp = fragment_program->program.get(mem);
        const auto parameters = gxp::program_parameters(*fragment_program_gxp);
        for (uint32_t i = 0; i < fragment_program_gxp->parameter_count; ++i) {
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

void sync_blending(const GxmContextState &state, const MemState &mem) {
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

void sync_vertex_attributes(GLContext &context, const GxmContextState &state, const MemState &mem) {
    // Vertex attributes.
    const SceGxmVertexProgram &vertex_program = *state.vertex_program.get(mem);
    for (const SceGxmVertexAttribute &attribute : vertex_program.attributes) {
        const GLVertexProgram *glvert = reinterpret_cast<GLVertexProgram *>(vertex_program.renderer_data.get());
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

            glBindBuffer(GL_ARRAY_BUFFER, context.stream_vertex_buffers[attribute.streamIndex]);

            if (upload_integral) {
                glVertexAttribIPointer(attrib_location, attribute.componentCount, type, stream.stride, reinterpret_cast<const GLvoid *>(attribute.offset));
            } else {
                glVertexAttribPointer(attrib_location, attribute.componentCount, type, normalised, stream.stride, reinterpret_cast<const GLvoid *>(attribute.offset));
            }

            glEnableVertexAttribArray(attrib_location);
        }
    }
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}
} // namespace renderer::gl
