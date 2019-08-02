#include <renderer/functions.h>

#include "functions.h"
#include "types.h"
#include <renderer/profile.h>

#include <renderer/state.h>
#include <renderer/types.h>

#include <gxm/functions.h>
#include <gxm/types.h>
#include <util/log.h>

#include <cassert>
#include <features/state.h>

namespace renderer::gl {
namespace texture {
bool init(GLTextureCacheState &cache) {
    cache.select_callback = [&](const std::size_t index) {
        const GLuint gl_texture = cache.textures[index];
        glBindTexture(GL_TEXTURE_2D, gl_texture);
    };

    cache.configure_texture_callback = [](const std::size_t index, const void *texture) {
        configure_bound_texture(*reinterpret_cast<const emu::SceGxmTexture *>(texture));
    };

    cache.upload_texture_callback = [](const std::size_t index, const void *texture, const MemState &mem) {
        upload_bound_texture(*reinterpret_cast<const emu::SceGxmTexture *>(texture), mem);
    };

    return cache.textures.init(glGenTextures, glDeleteTextures);
}
} // namespace texture

static GLenum translate_blend_func(SceGxmBlendFunc src) {
    R_PROFILE(__func__);

    switch (src) {
    case SCE_GXM_BLEND_FUNC_NONE:
        return GL_FUNC_ADD; // TODO Disable blending? Warn?
    case SCE_GXM_BLEND_FUNC_ADD:
        return GL_FUNC_ADD;
    case SCE_GXM_BLEND_FUNC_SUBTRACT:
        return GL_FUNC_SUBTRACT;
    case SCE_GXM_BLEND_FUNC_REVERSE_SUBTRACT:
        return GL_FUNC_REVERSE_SUBTRACT;
    case SCE_GXM_BLEND_FUNC_MIN:
        return GL_MIN;
    case SCE_GXM_BLEND_FUNC_MAX:
        return GL_MAX;
    }

    return GL_FUNC_ADD;
}

static GLenum translate_blend_factor(SceGxmBlendFactor src) {
    R_PROFILE(__func__);

    switch (src) {
    case SCE_GXM_BLEND_FACTOR_ZERO:
        return GL_ZERO;
    case SCE_GXM_BLEND_FACTOR_ONE:
        return GL_ONE;
    case SCE_GXM_BLEND_FACTOR_SRC_COLOR:
        return GL_SRC_COLOR;
    case SCE_GXM_BLEND_FACTOR_ONE_MINUS_SRC_COLOR:
        return GL_ONE_MINUS_SRC_COLOR;
    case SCE_GXM_BLEND_FACTOR_SRC_ALPHA:
        return GL_SRC_ALPHA;
    case SCE_GXM_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA:
        return GL_ONE_MINUS_SRC_ALPHA;
    case SCE_GXM_BLEND_FACTOR_DST_COLOR:
        return GL_DST_COLOR;
    case SCE_GXM_BLEND_FACTOR_ONE_MINUS_DST_COLOR:
        return GL_ONE_MINUS_DST_COLOR;
    case SCE_GXM_BLEND_FACTOR_DST_ALPHA:
        return GL_DST_ALPHA;
    case SCE_GXM_BLEND_FACTOR_ONE_MINUS_DST_ALPHA:
        return GL_ONE_MINUS_DST_ALPHA;
    case SCE_GXM_BLEND_FACTOR_SRC_ALPHA_SATURATE:
        return GL_SRC_ALPHA_SATURATE;
    case SCE_GXM_BLEND_FACTOR_DST_ALPHA_SATURATE:
        return GL_DST_ALPHA; // TODO Not supported.
    }

    return GL_ONE;
}

static AttributeLocations attribute_locations(const SceGxmProgram &vertex_program) {
    R_PROFILE(__func__);
    AttributeLocations locations;

    const SceGxmProgramParameter *const parameters = gxp::program_parameters(vertex_program);
    for (uint32_t i = 0; i < vertex_program.parameter_count; ++i) {
        const SceGxmProgramParameter &parameter = parameters[i];
        if (parameter.category == SCE_GXM_PARAMETER_CATEGORY_ATTRIBUTE) {
            std::string name = gxp::parameter_name_raw(parameter);
            const auto struct_idx = name.find('.');
            const bool is_struct_field = struct_idx != std::string::npos;
            if (is_struct_field)
                name.replace(struct_idx, 1, "_"); // GLSL vertex inputs can't be structs, replace them here and in shader translator
            locations.emplace(parameter.resource_index, name);
        }
    }

    return locations;
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

void bind_fundamental(GLContext &context) {
    // Bind the vertex array and element buffer.
    glBindVertexArray(context.vertex_array[0]);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, context.element_buffer[0]);
}

bool create(std::unique_ptr<Context> &context) {
    R_PROFILE(__func__);

    context = std::make_unique<GLContext>();
    GLContext *gl_context = reinterpret_cast<GLContext *>(context.get());

    if (!texture::init(gl_context->texture_cache) || !gl_context->vertex_array.init(glGenVertexArrays, glDeleteVertexArrays) || !gl_context->element_buffer.init(glGenBuffers, glDeleteBuffers) || !gl_context->stream_vertex_buffers.init(glGenBuffers, glDeleteBuffers)) {
        return false;
    }

    return true;
}

bool create(std::unique_ptr<RenderTarget> &rt, const SceGxmRenderTargetParams &params, const FeatureState &features) {
    R_PROFILE(__func__);

    rt = std::make_unique<GLRenderTarget>();
    GLRenderTarget *render_target = reinterpret_cast<GLRenderTarget *>(rt.get());

    if (!render_target->renderbuffers.init(glGenRenderbuffers, glDeleteRenderbuffers) || !render_target->framebuffer.init(glGenFramebuffers, glDeleteFramebuffers)) {
        return false;
    }

    int depth_fb_index = 1;

    if (features.is_programmable_blending_need_to_bind_color_attachment()) {
        depth_fb_index = 0;
        render_target->color_attachment.init(glGenTextures, glDeleteTextures);

        glBindTexture(GL_TEXTURE_2D, render_target->color_attachment[0]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, params.width, params.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    } else {
        glBindRenderbuffer(GL_RENDERBUFFER, render_target->renderbuffers[0]);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA, params.width, params.height);
    }

    glBindRenderbuffer(GL_RENDERBUFFER, render_target->renderbuffers[depth_fb_index]);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, params.width, params.height);
    glBindRenderbuffer(GL_RENDERBUFFER, 0);

    if (features.is_programmable_blending_need_to_bind_color_attachment()) {
        // Attach it to the framebuffer
        glBindFramebuffer(GL_FRAMEBUFFER, render_target->framebuffer[0]);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, render_target->color_attachment[0], 0);
    } else {
        glBindFramebuffer(GL_FRAMEBUFFER, render_target->framebuffer[0]);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, render_target->renderbuffers[0]);
    }

    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, render_target->renderbuffers[depth_fb_index]);
    glClearColor(0.968627450f, 0.776470588f, 0.0f, 1.0f);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    return true;
}

bool create(std::unique_ptr<FragmentProgram> &fp, GLState &state, const SceGxmProgram &program, const emu::SceGxmBlendInfo *blend, GXPPtrMap &gxp_ptr_map, const char *base_path, const char *title_id) {
    R_PROFILE(__func__);

    fp = std::make_unique<GLFragmentProgram>();
    GLFragmentProgram *frag_program_gl = reinterpret_cast<GLFragmentProgram *>(fp.get());

    // Try to hash this shader
    const Sha256Hash hash_bytes_f = sha256(&program, program.size);
    fp->hash.assign(hash_bytes_f.begin(), hash_bytes_f.end());

    gxp_ptr_map.emplace(hash_bytes_f, &program);

    // Translate blending.
    if (blend != nullptr) {
        frag_program_gl->color_mask_red = ((blend->colorMask & SCE_GXM_COLOR_MASK_R) != 0) ? GL_TRUE : GL_FALSE;
        frag_program_gl->color_mask_green = ((blend->colorMask & SCE_GXM_COLOR_MASK_G) != 0) ? GL_TRUE : GL_FALSE;
        frag_program_gl->color_mask_blue = ((blend->colorMask & SCE_GXM_COLOR_MASK_B) != 0) ? GL_TRUE : GL_FALSE;
        frag_program_gl->color_mask_alpha = ((blend->colorMask & SCE_GXM_COLOR_MASK_A) != 0) ? GL_TRUE : GL_FALSE;
        frag_program_gl->blend_enabled = true;
        frag_program_gl->color_func = translate_blend_func(blend->colorFunc);
        frag_program_gl->alpha_func = translate_blend_func(blend->alphaFunc);
        frag_program_gl->color_src = translate_blend_factor(blend->colorSrc);
        frag_program_gl->color_dst = translate_blend_factor(blend->colorDst);
        frag_program_gl->alpha_src = translate_blend_factor(blend->alphaSrc);
        frag_program_gl->alpha_dst = translate_blend_factor(blend->alphaDst);
    }

    return true;
}

bool create(std::unique_ptr<VertexProgram> &vp, GLState &state, const SceGxmProgram &program, GXPPtrMap &gxp_ptr_map, const char *base_path, const char *title_id) {
    R_PROFILE(__func__);

    vp = std::make_unique<GLVertexProgram>();
    GLVertexProgram *vert_program_gl = reinterpret_cast<GLVertexProgram *>(vp.get());

    // Try to hash this shader
    const Sha256Hash hash_bytes_v = sha256(&program, program.size);
    vp->hash.assign(hash_bytes_v.begin(), hash_bytes_v.end());
    gxp_ptr_map.emplace(hash_bytes_v, &program);

    vert_program_gl->attribute_locations = attribute_locations(program);

    return true;
}

void set_context(GLContext &context, const GLRenderTarget *rt, const FeatureState &features) {
    R_PROFILE(__func__);

    bind_fundamental(context);

    if (rt) {
        sync_rendertarget(*rt);
    } else {
        sync_rendertarget(*reinterpret_cast<const GLRenderTarget *>(context.current_render_target));
    }

    // Bind it for programmable blending
    if (features.is_programmable_blending_need_to_bind_color_attachment()) {
        if (features.should_use_shader_interlock())
            glBindImageTexture(COLOR_ATTACHMENT_TEXTURE_SLOT_IMAGE, rt->color_attachment[0], 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA8);
        else {
            // Hopefully no one will use slot 12
            // TODO: Move color attachment futher or try to preserve it
            glActiveTexture(GL_TEXTURE0 + COLOR_ATTACHMENT_TEXTURE_SLOT_SAMPLER);
            glBindTexture(GL_TEXTURE_2D, rt->color_attachment[0]);
        }
    }

    // TODO This is just for debugging.
    // glClear(GL_COLOR_BUFFER_BIT);
}

void get_surface_data(GLContext &context, size_t width, size_t height, size_t stride_in_pixels, uint32_t *pixels) {
    R_PROFILE(__func__);

    if (pixels == nullptr) {
        return;
    }

    glPixelStorei(GL_PACK_ROW_LENGTH, static_cast<GLint>(stride_in_pixels));
    glReadPixels(0, 0, static_cast<GLsizei>(width), static_cast<GLsizei>(height), GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    glPixelStorei(GL_PACK_ROW_LENGTH, 0);

    flip_vertically(pixels, width, height, stride_in_pixels);

    ++context.texture_cache.timestamp;
}

void upload_vertex_stream(GLContext &context, const std::size_t stream_index, const std::size_t length, const void *data) {
    glBindBuffer(GL_ARRAY_BUFFER, context.stream_vertex_buffers[stream_index]);

    // Orphan the buffer, so we don't have to stall the pipeline, wait for last draw call to finish
    // See OpenGL Insight at 28.3.2. Intel driver likes glBufferData more, so use glBufferData.
    glBufferData(GL_ARRAY_BUFFER, length, nullptr, GL_DYNAMIC_DRAW);
    glBufferData(GL_ARRAY_BUFFER, length, data, GL_DYNAMIC_DRAW);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

} // namespace renderer::gl
