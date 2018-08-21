#include <renderer/functions.h>

#include "functions.h"
#include "profile.h"

#include <renderer/state.h>
#include <renderer/types.h>

#include <gxm/functions.h>
#include <gxm/types.h>
#include <util/log.h>

#include <SDL_video.h>
#include <glbinding/Binding.h>

#include <cassert>

using namespace glbinding;

namespace renderer {
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

    // TODO This is just for debugging.
    glClearColor(0.0625f, 0.125f, 0.25f, 0);

    if (!texture::init(context.texture_cache) || !context.vertex_array.init(glGenVertexArrays, glDeleteVertexArrays) || !context.element_buffer.init(glGenBuffers, glDeleteBuffers) || !context.stream_vertex_buffers.init(glGenBuffers, glDeleteBuffers)) {
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

bool create(FragmentProgram &fp, State &state, const SceGxmProgram &program, const emu::SceGxmBlendInfo *blend, const char *base_path) {
    R_PROFILE(__func__);

    fp.glsl = load_fragment_shader(state.fragment_glsl_cache, program, base_path);

    // Translate blending.
    if (blend != nullptr) {
        fp.color_mask_red = ((blend->colorMask & SCE_GXM_COLOR_MASK_R) != 0) ? GL_TRUE : GL_FALSE;
        fp.color_mask_green = ((blend->colorMask & SCE_GXM_COLOR_MASK_G) != 0) ? GL_TRUE : GL_FALSE;
        fp.color_mask_blue = ((blend->colorMask & SCE_GXM_COLOR_MASK_B) != 0) ? GL_TRUE : GL_FALSE;
        fp.color_mask_alpha = ((blend->colorMask & SCE_GXM_COLOR_MASK_A) != 0) ? GL_TRUE : GL_FALSE;
        fp.blend_enabled = true;
        fp.color_func = translate_blend_func(blend->colorFunc);
        fp.alpha_func = translate_blend_func(blend->alphaFunc);
        fp.color_src = translate_blend_factor(blend->colorSrc);
        fp.color_dst = translate_blend_factor(blend->colorDst);
        fp.alpha_src = translate_blend_factor(blend->alphaSrc);
        fp.alpha_dst = translate_blend_factor(blend->alphaDst);
    }

    return true;
}

bool create(VertexProgram &vp, State &state, const SceGxmProgram &program, const char *base_path) {
    R_PROFILE(__func__);

    vp.glsl = load_vertex_shader(state.vertex_glsl_cache, program, base_path);
    vp.attribute_locations = attribute_locations(program);

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

void finish(Context &context) {
    glFinish();
}
} // namespace renderer
