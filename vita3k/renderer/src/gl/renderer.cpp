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
#include <renderer/state.h>
#include <renderer/types.h>

#include <renderer/gl/functions.h>
#include <renderer/gl/types.h>

#include <shader/spirv_recompiler.h>
#include <shader/usse_program_analyzer.h>

#include <display/state.h>
#include <features/state.h>
#include <gxm/state.h>

#include <gxm/functions.h>
#include <gxm/types.h>
#include <util/log.h>

#include <SDL3/SDL_video.h>

#include <array>
#include <mutex>
#include <string_view>

namespace renderer::gl {

GLContext::GLContext()
    : vertex_stream_ring_buffer(GL_ARRAY_BUFFER, MiB(128))
    , index_stream_ring_buffer(GL_ELEMENT_ARRAY_BUFFER, MiB(64))
    , vertex_uniform_stream_ring_buffer(GL_SHADER_STORAGE_BUFFER, MiB(256))
    , fragment_uniform_stream_ring_buffer(GL_SHADER_STORAGE_BUFFER, MiB(256))
    , vertex_info_uniform_buffer(GL_UNIFORM_BUFFER, MiB(8))
    , fragment_info_uniform_buffer(GL_UNIFORM_BUFFER, MiB(8)) {
    std::memset(&previous_vert_info, 0, sizeof(shader::RenderVertUniformBlock));
    std::memset(&previous_frag_info, 0, sizeof(shader::RenderFragUniformBlock));
}

static GLenum translate_blend_func(SceGxmBlendFunc src) {
    R_PROFILE(__func__);

    switch (src) {
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
    default:
        return GL_FUNC_ADD;
    }
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
    default:
        return GL_ZERO;
    }
}

void bind_fundamental(GLContext &context) {
    // Bind the vertex array and element buffer.
    glBindVertexArray(context.vertex_array[0]);
}

static void after_callback(void *ret, const char *name, GLADapiproc apiproc, int len_args, ...) {
    GLAD_UNUSED(ret);
    GLAD_UNUSED(apiproc);
    GLAD_UNUSED(len_args);

    GLenum error_code = glad_glGetError();

    if (error_code != GL_NO_ERROR) {
        LOG_ERROR("OpenGL: {} set error {}.", name, error_code);
    }
}

#ifndef NDEBUG
static void debug_output_callback(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length,
    const GLchar *message, const void *userParam) {
    const char *type_str = nullptr;

    switch (type) {
    case GL_DEBUG_TYPE_ERROR:
        type_str = "ERROR";
        break;
    case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR:
        type_str = "DEPRECATED_BEHAVIOR";
        break;
    case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:
        type_str = "UNDEFINED_BEHAVIOR";
        break;
    case GL_DEBUG_TYPE_PORTABILITY:
        type_str = "PORTABILITY";
        break;
    case GL_DEBUG_TYPE_PERFORMANCE:
        type_str = "PERFORMANCE";
        break;
    case GL_DEBUG_TYPE_OTHER:
        type_str = "OTHER";
        break;
    default:
        type_str = "UNKTYPE";
        break;
    }

    const char *severity_fmt = nullptr;
    switch (severity) {
    case GL_DEBUG_SEVERITY_LOW:
        severity_fmt = "LOW";
        break;
    case GL_DEBUG_SEVERITY_MEDIUM:
        severity_fmt = "MEDIUM";
        break;
    case GL_DEBUG_SEVERITY_HIGH:
        severity_fmt = "HIGH";
        break;
    default:
        severity_fmt = "UNKSERV";
        break;
    }

    LOG_DEBUG("[OPENGL - {} - {}] {}", type_str, severity_fmt, message);
}
#endif

bool create(SDL_Window *window, std::unique_ptr<State> &state, const Config &config) {
    auto &gl_state = dynamic_cast<GLState &>(*state);

    // Recursively create GL version until one accepts
    // Major 4 is mandatory
    // We use glBufferStorage which needs OpenGL 4.4
    constexpr std::array accept_gl_minor_versions = {
        6, // OpenGL 4.6
        5, // OpenGL 4.5
        4, // OpenGL 4.4
    };

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
#ifndef NDEBUG
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_DEBUG_FLAG);
#endif

    for (int minor_version : accept_gl_minor_versions) {
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, minor_version);
        gl_state.context = GLContextPtr(SDL_GL_CreateContext(window), [](SDL_GLContext context) { SDL_GL_DestroyContext(context); });
        if (gl_state.context) {
            break;
        }
    }

    if (!gl_state.context)
        return false;

    if (!gladLoadGL((GLADloadfunc)SDL_GL_GetProcAddress))
        return false;

    gladSetGLPostCallback(after_callback);

    // Detect GPU and features
    const std::string gpu_name = reinterpret_cast<const GLchar *>(glGetString(GL_RENDERER));
    const std::string version = reinterpret_cast<const GLchar *>(glGetString(GL_SHADING_LANGUAGE_VERSION));

    LOG_INFO("GPU = {}", gpu_name);
    LOG_INFO("GL_VERSION = {}", reinterpret_cast<const char *>(glGetString(GL_VERSION)));
    LOG_INFO("GL_SHADING_LANGUAGE_VERSION = {}", version);

#ifndef NDEBUG
    glDebugMessageCallback(reinterpret_cast<GLDEBUGPROC>(debug_output_callback), nullptr);
#endif

    int total_extensions = 0;
    glGetIntegerv(GL_NUM_EXTENSIONS, &total_extensions);

    std::unordered_map<std::string, bool *> check_extensions = {
        { "GL_ARB_fragment_shader_interlock", &gl_state.features.support_shader_interlock },
        { "GL_ARB_texture_barrier", &gl_state.features.support_texture_barrier },
        { "GL_EXT_shader_framebuffer_fetch", &gl_state.features.direct_fragcolor },
        { "GL_ARB_gl_spirv", &gl_state.features.spirv_shader },
        { "GL_ARB_get_texture_sub_image", &gl_state.features.support_get_texture_sub_image },
        { "GL_EXT_shader_image_load_formatted", &gl_state.features.support_unknown_format }
    };

    for (int i = 0; i < total_extensions; i++) {
        const std::string extension = reinterpret_cast<const GLchar *>(glGetStringi(GL_EXTENSIONS, i));
        auto find_result = check_extensions.find(extension);

        if (find_result != check_extensions.end()) {
            *find_result->second = true;
            check_extensions.erase(find_result);
        }
    }

    if (gl_state.features.direct_fragcolor) {
        LOG_INFO("Your GPU supports direct access to last fragment color. Your performance with programmable blending games will be optimized.");
    } else if (gl_state.features.support_shader_interlock) {
        LOG_INFO("Your GPU supports shader interlock, some games that use programmable blending will have better performance.");
    } else if (gl_state.features.support_texture_barrier) {
        LOG_INFO("Your GPU only supports texture barrier, performance may not be good on programmable blending games.");
        LOG_WARN("Consider upgrading to a GPU that has shader interlock.");
    } else {
        LOG_INFO("Your GPU doesn't support extensions that make programmable blending possible. Some games may have broken graphics.");
        LOG_WARN("Consider updating your graphics drivers or upgrading your GPU.");
    }

    // always enabled in the opengl renderer
    gl_state.features.use_mask_bit = true;

    return gl_state.init();
}

bool GLState::init() {
    if (!screen_renderer.init(static_assets)) {
        LOG_ERROR("Failed to initialize screen renderer");
        return false;
    }

    shader_version = fmt::format("v{}", shader::CURRENT_VERSION);

    return true;
}

void GLState::late_init(const Config &cfg, const std::string_view game_id, MemState &mem) {
    texture_cache.init(cfg.hashless_texture_cache, texture_folder(), game_id);
}

bool create(std::unique_ptr<Context> &context) {
    R_PROFILE(__func__);

    context = std::make_unique<GLContext>();
    GLContext *gl_context = reinterpret_cast<GLContext *>(context.get());

    const bool init_result = gl_context->vertex_array.init(glGenVertexArrays, glDeleteVertexArrays);

    if (!init_result) {
        return false;
    }

    return true;
}

bool create(GLState &state, std::unique_ptr<RenderTarget> &rt, const SceGxmRenderTargetParams &params, const FeatureState &features) {
    R_PROFILE(__func__);

    rt = std::make_unique<GLRenderTarget>();
    GLRenderTarget *render_target = reinterpret_cast<GLRenderTarget *>(rt.get());

    if (!render_target->maskbuffer.init(glGenFramebuffers, glDeleteFramebuffers)) {
        return false;
    }

    render_target->width = static_cast<uint16_t>(params.width * state.res_multiplier);
    render_target->height = static_cast<uint16_t>(params.height * state.res_multiplier);

    render_target->attachments.init(glGenTextures, glDeleteTextures);

    glBindTexture(GL_TEXTURE_2D, render_target->attachments[0]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, render_target->width, render_target->height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glBindTexture(GL_TEXTURE_2D, render_target->attachments[1]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH24_STENCIL8, render_target->width, render_target->height, 0, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, nullptr);

    render_target->masktexture.init(glGenTextures, glDeleteTextures);
    glBindTexture(GL_TEXTURE_2D, render_target->masktexture[0]);
    // we need to make the masktexture format immutable, otherwise image load operations
    // won't work on mesa drivers
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, render_target->width, render_target->height);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindFramebuffer(GL_FRAMEBUFFER, render_target->maskbuffer[0]);
    glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, render_target->masktexture[0], 0);
    GLenum drawbuffers[1] = { GL_COLOR_ATTACHMENT0 };
    glDrawBuffers(1, drawbuffers);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    return true;
}

bool create(std::unique_ptr<FragmentProgram> &fp, GLState &state, const SceGxmProgram &program, const SceGxmBlendInfo *blend) {
    R_PROFILE(__func__);

    fp = std::make_unique<GLFragmentProgram>();
    GLFragmentProgram *frag_program_gl = reinterpret_cast<GLFragmentProgram *>(fp.get());

    // Translate blending.
    if (blend != nullptr) {
        frag_program_gl->color_mask_red = ((blend->colorMask & SCE_GXM_COLOR_MASK_R) != 0) ? GL_TRUE : GL_FALSE;
        frag_program_gl->color_mask_green = ((blend->colorMask & SCE_GXM_COLOR_MASK_G) != 0) ? GL_TRUE : GL_FALSE;
        frag_program_gl->color_mask_blue = ((blend->colorMask & SCE_GXM_COLOR_MASK_B) != 0) ? GL_TRUE : GL_FALSE;
        frag_program_gl->color_mask_alpha = ((blend->colorMask & SCE_GXM_COLOR_MASK_A) != 0) ? GL_TRUE : GL_FALSE;
        frag_program_gl->blend_enabled = (blend->colorFunc != SCE_GXM_BLEND_FUNC_NONE) || (blend->alphaFunc != SCE_GXM_BLEND_FUNC_NONE);
        frag_program_gl->color_func = translate_blend_func(blend->colorFunc);
        frag_program_gl->color_src = translate_blend_factor(blend->colorSrc);
        frag_program_gl->color_dst = translate_blend_factor(blend->colorDst);
        frag_program_gl->alpha_func = translate_blend_func(blend->alphaFunc);
        frag_program_gl->alpha_src = translate_blend_factor(blend->alphaSrc);
        frag_program_gl->alpha_dst = translate_blend_factor(blend->alphaDst);
    }

    return true;
}

bool create(std::unique_ptr<VertexProgram> &vp, GLState &state, const SceGxmProgram &program) {
    R_PROFILE(__func__);
    vp = std::make_unique<GLVertexProgram>();

    return true;
}

void set_context(GLState &state, GLContext &context, const MemState &mem, const GLRenderTarget *rt, const FeatureState &features) {
    R_PROFILE(__func__);

    bind_fundamental(context);

    if (rt) {
        context.render_target = rt;
    } else {
        context.render_target = reinterpret_cast<const GLRenderTarget *>(context.current_render_target);
    }

    state.surface_cache.set_render_target(context.render_target);

    SceGxmColorSurface *color_surface_fin = &context.record.color_surface;
    if (color_surface_fin->data.address() == 0) {
        color_surface_fin = nullptr;
    }

    SceGxmDepthStencilSurface *ds_surface_fin = &context.record.depth_stencil_surface;
    if ((ds_surface_fin->depth_data.address() == 0) && (ds_surface_fin->stencil_data.address() == 0)) {
        ds_surface_fin = nullptr;
    }

    GLuint current_color_attachment_handle = 0;
    std::uint16_t current_framebuffer_height = 0;

    context.current_framebuffer = state.surface_cache.retrieve_framebuffer_handle(
        state, mem, color_surface_fin, ds_surface_fin, &current_color_attachment_handle, nullptr, &current_framebuffer_height);
    context.current_color_attachment = current_color_attachment_handle;
    context.current_framebuffer_height = current_framebuffer_height;

    glBindFramebuffer(GL_FRAMEBUFFER, context.current_framebuffer);

    if (context.record.region_clip_mode != SCE_GXM_REGION_CLIP_NONE) {
        glDisable(GL_SCISSOR_TEST);
    }

    sync_mask(state, context, mem);

    // TODO: Take request to force load from given memory
    // Sync depth/stencil based on depth stencil surface.
    sync_depth_data(context.record);
    sync_depth_func(context.record.front_depth_func, true);
    sync_depth_func(context.record.back_depth_func, false);
    sync_depth_write_enable(context.record.front_depth_write_mode, true);
    sync_depth_write_enable(context.record.back_depth_write_mode, false);

    sync_stencil_data(context.record, mem);
    sync_stencil_func(context.record.back_stencil_state_op, context.record.back_stencil_state_values, mem, true);
    sync_stencil_func(context.record.front_stencil_state_op, context.record.front_stencil_state_values, mem, false);

    if (context.record.region_clip_mode != SCE_GXM_REGION_CLIP_NONE) {
        glEnable(GL_SCISSOR_TEST);
    }
}

static std::map<SceGxmColorFormat, std::pair<GLenum, GLenum>> GXM_COLOR_FORMAT_TO_GL_FORMAT = {
    { SCE_GXM_COLOR_FORMAT_U8U8U8U8_ABGR, { GL_RGBA, GL_UNSIGNED_INT_8_8_8_8_REV } },
    { SCE_GXM_COLOR_FORMAT_U8U8U8U8_ARGB, { GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV } },
    { SCE_GXM_COLOR_FORMAT_U8U8U8U8_RGBA, { GL_RGBA, GL_UNSIGNED_BYTE } },
    { SCE_GXM_COLOR_FORMAT_U4U4U4U4_ARGB, { GL_BGRA, GL_UNSIGNED_SHORT_4_4_4_4_REV } },
    { SCE_GXM_COLOR_FORMAT_U8U8U8_BGR, { GL_RGB, GL_UNSIGNED_BYTE } },
    { SCE_GXM_COLOR_FORMAT_U5U6U5_RGB, { GL_RGB, GL_UNSIGNED_SHORT_5_6_5 } },
    { SCE_GXM_COLOR_FORMAT_U8U8_AR, { GL_RG, GL_UNSIGNED_BYTE } },
    { SCE_GXM_COLOR_FORMAT_U8_A, { GL_ALPHA, GL_UNSIGNED_BYTE } },
    { SCE_GXM_COLOR_FORMAT_U8_R, { GL_RED, GL_UNSIGNED_BYTE } },
    { SCE_GXM_COLOR_FORMAT_U2F10F10F10_ABGR, { GL_RGBA, GL_UNSIGNED_INT_2_10_10_10_REV } },
    { SCE_GXM_COLOR_FORMAT_U2U10U10U10_ABGR, { GL_RGBA, GL_UNSIGNED_INT_2_10_10_10_REV } },
    { SCE_GXM_COLOR_FORMAT_U10U10U10U2_RGBA, { GL_BGRA, GL_UNSIGNED_INT_10_10_10_2 } },
    { SCE_GXM_COLOR_FORMAT_U10U10U10U2_BGRA, { GL_RGBA, GL_UNSIGNED_INT_10_10_10_2 } },
    { SCE_GXM_COLOR_FORMAT_F16_R, { GL_RED, GL_HALF_FLOAT } },
    { SCE_GXM_COLOR_FORMAT_F16F16_GR, { GL_RG, GL_HALF_FLOAT } },
    { SCE_GXM_COLOR_FORMAT_F16F16F16F16_ABGR, { GL_RGBA, GL_UNSIGNED_SHORT } },
    { SCE_GXM_COLOR_FORMAT_F16F16F16F16_ARGB, { GL_BGRA, GL_UNSIGNED_SHORT } },
    { SCE_GXM_COLOR_FORMAT_F32_R, { GL_RED, GL_FLOAT } },
    { SCE_GXM_COLOR_FORMAT_F32F32_GR, { GL_RG, GL_FLOAT } },
    { SCE_GXM_COLOR_FORMAT_F11F11F10_RGB, { GL_RGB, GL_UNSIGNED_INT_10F_11F_11F_REV } },
    { SCE_GXM_COLOR_FORMAT_SE5M9M9M9_BGR, { GL_RGB, GL_HALF_FLOAT } },
    { SCE_GXM_COLOR_FORMAT_SE5M9M9M9_RGB, { GL_BGR, GL_HALF_FLOAT } }
};

static bool format_need_temp_storage(const GLState &state, SceGxmColorSurface &surface, std::vector<std::uint8_t> &storage, const std::uint32_t width, const std::uint32_t height) {
    size_t needed_pixels;
    if (state.res_multiplier == 1.0f) {
        needed_pixels = surface.strideInPixels * height;
    } else {
        // width and height is already upscaled
        needed_pixels = width * height;
    }

    if ((surface.colorFormat == SCE_GXM_COLOR_FORMAT_SE5M9M9M9_BGR) || (surface.colorFormat == SCE_GXM_COLOR_FORMAT_SE5M9M9M9_RGB)) {
        storage.resize(needed_pixels * 3 * 2); // RGB and half float
        return true;
    }

    if (state.res_multiplier > 1.0f) {
        storage.resize(needed_pixels * gxm::bits_per_pixel(gxm::get_base_format(surface.colorFormat)) >> 3);
        return true;
    }

    return false;
}

static void post_process_pixels_data(GLState &renderer, std::uint32_t *pixels, std::uint8_t *source, std::uint32_t width, std::uint32_t height, const std::uint32_t stride,
    SceGxmColorSurface &surface) {
    uint8_t *curr_input = source;
    uint8_t *curr_output = reinterpret_cast<uint8_t *>(pixels);

    const bool is_U8U8U8_RGBA = surface.colorFormat == SCE_GXM_COLOR_FORMAT_U8U8U8U8_RGBA;
    const bool is_SE5M9M9M9 = (surface.colorFormat == SCE_GXM_COLOR_FORMAT_SE5M9M9M9_RGB) || (surface.colorFormat == SCE_GXM_COLOR_FORMAT_SE5M9M9M9_BGR);

    const int multiplier = static_cast<int>(renderer.res_multiplier);
    if (multiplier > 1 || is_U8U8U8_RGBA || is_SE5M9M9M9) {
        // TODO: do this on the GPU instead (using texture blitting?)
        const int bytes_per_output_pixel = (gxm::bits_per_pixel(gxm::get_base_format(surface.colorFormat)) + 7) >> 3;
        const int bytes_per_input_pixel = is_SE5M9M9M9 ? 6 : bytes_per_output_pixel;

        const uint32_t end_stride_bytes = bytes_per_output_pixel * (surface.strideInPixels - surface.width);
        for (uint32_t row = 0; row < surface.height; row++) {
            for (uint32_t col = 0; col < surface.width; col++) {
                if (!is_SE5M9M9M9) {
                    memcpy(curr_output, curr_input, bytes_per_input_pixel);

                    if (is_U8U8U8_RGBA) {
                        std::swap(curr_input[0], curr_input[3]);
                        std::swap(curr_input[1], curr_input[2]);
                    }
                } else {
                    const uint16_t *temp_bytes = reinterpret_cast<uint16_t *>(curr_input);
                    uint32_t pixel = 0;
                    pixel |= static_cast<uint32_t>(temp_bytes[0] << 17) & (0x3FFF << 18); // Exp + 9 bits
                    pixel |= static_cast<uint32_t>(temp_bytes[1] << 8) & (0x1FF << 9);
                    pixel |= static_cast<uint32_t>(temp_bytes[2] >> 1) & (0x1FF << 0);
                    *reinterpret_cast<uint32_t *>(curr_output) = pixel;
                }

                curr_output += bytes_per_output_pixel;
                curr_input += bytes_per_input_pixel * multiplier;
            }
            // take the pixel stride into account
            curr_output += end_stride_bytes;
            curr_input += (multiplier - 1) * width * bytes_per_input_pixel;
        }

        width /= multiplier;
        height /= multiplier;
    }

    if (surface.surfaceType == SCE_GXM_COLOR_SURFACE_TILED) {
        const SceGxmColorBaseFormat base_format = gxm::get_base_format(surface.colorFormat);
        const size_t bpp = gxm::bits_per_pixel(base_format);
        const size_t bytes_per_pixel = (bpp + 7) >> 3;
        std::vector<uint8_t> buffer;

        buffer.resize(((width + 31) / 32) * ((height + 31) / 32) * 1024 * bytes_per_pixel);
        for (uint32_t j = 0; j < height; j++) {
            for (uint32_t hori_tile = 0; hori_tile < (width >> 5); hori_tile++) {
                const size_t tile_position = hori_tile + (j >> 5) * ((width + 31) >> 5);
                const size_t first_pixel_offset_in_tile = (tile_position << 10) + (j & 31) * 32;
                const size_t first_pixel_offset_in_linear = (j * stride) + hori_tile * 32;

                memcpy(buffer.data() + first_pixel_offset_in_tile * bytes_per_pixel,
                    (const char *)(pixels) + first_pixel_offset_in_linear * bytes_per_pixel, 32 * bytes_per_pixel);
            }
        }
        memcpy(pixels, buffer.data(), buffer.size());
    }
}

void lookup_and_get_surface_data(GLState &renderer, MemState &mem, SceGxmColorSurface &surface) {
    std::uint32_t swizzle = 0;

    GLint tex_handle = static_cast<GLint>(renderer.surface_cache.retrieve_color_surface_texture_handle(renderer, static_cast<std::uint16_t>(surface.width),
        static_cast<std::uint16_t>(surface.height), static_cast<std::uint16_t>(surface.strideInPixels),
        gxm::get_base_format(surface.colorFormat), surface.data, SurfaceTextureRetrievePurpose::READING, swizzle));

    if (tex_handle == 0) {
        return;
    }

    SceGxmColorFormat format = surface.colorFormat;
    uint32_t width = surface.width;
    uint32_t height = surface.height;
    size_t buffer_size;

    if (renderer.res_multiplier == 1) {
        glPixelStorei(GL_PACK_ROW_LENGTH, static_cast<GLint>(surface.strideInPixels));
        buffer_size = gxm::get_stride_in_bytes(surface.colorFormat, surface.strideInPixels) * height;
    } else {
        width *= renderer.res_multiplier;
        height *= renderer.res_multiplier;
        glPixelStorei(GL_PACK_ROW_LENGTH, static_cast<GLint>(width));
        buffer_size = gxm::get_stride_in_bytes(surface.colorFormat, width) * height;
    }

    auto format_gl = GXM_COLOR_FORMAT_TO_GL_FORMAT.find(format);
    if (format_gl == GXM_COLOR_FORMAT_TO_GL_FORMAT.end()) {
        LOG_ERROR("Color format not implemented: {}, report this to developer", fmt::underlying(format));
        return;
    }

    std::uint32_t *pixels = surface.data.cast<std::uint32_t>().get(mem);
    std::uint8_t *temp_store = reinterpret_cast<std::uint8_t *>(pixels);
    if (!pixels) {
        return;
    }

    std::vector<std::uint8_t> storage_v;
    if (format_need_temp_storage(renderer, surface, storage_v, width, height)) {
        temp_store = storage_v.data();
        buffer_size = storage_v.size();
    }

    const SceGxmColorBaseFormat base_format = gxm::get_base_format(format);

    GLenum gl_format = format_gl->second.first;
    GLenum gl_type = format_gl->second.second;
    if (renderer.features.preserve_f16_nan_as_u16 && color::is_write_surface_stored_rawly(base_format)) {
        gl_format = color::get_raw_store_upload_format_type(base_format);
        gl_type = color::get_raw_store_upload_data_type(base_format);
    }

    if (renderer.features.support_get_texture_sub_image) {
        glGetTextureSubImage(tex_handle, 0, 0, 0, 0, width, height, 1, gl_format, gl_type, buffer_size, temp_store);
    } else {
        GLint last_texture = 0;

        glGetIntegerv(GL_TEXTURE_BINDING_2D, &last_texture);
        glBindTexture(GL_TEXTURE_2D, tex_handle);
        glGetTexImage(GL_TEXTURE_2D, 0, gl_format, gl_type, temp_store);
        glBindTexture(GL_TEXTURE_2D, last_texture);
    }

    post_process_pixels_data(renderer, pixels, temp_store, width, height, surface.strideInPixels, surface);
    glPixelStorei(GL_PACK_ROW_LENGTH, 0);
}

void get_surface_data(GLState &renderer, GLContext &context, uint32_t *pixels, SceGxmColorSurface &surface) {
    R_PROFILE(__func__);

    if (pixels == nullptr) {
        return;
    }

    SceGxmColorFormat format = surface.colorFormat;
    uint32_t width = surface.width;
    uint32_t height = surface.height;

    const int res_multiplier = static_cast<int>(renderer.res_multiplier);
    if (res_multiplier == 1) {
        glPixelStorei(GL_PACK_ROW_LENGTH, static_cast<GLint>(surface.strideInPixels));
    } else {
        width *= res_multiplier;
        height *= res_multiplier;
        glPixelStorei(GL_PACK_ROW_LENGTH, static_cast<GLint>(width));
    }

    auto format_gl = GXM_COLOR_FORMAT_TO_GL_FORMAT.find(format);
    if (format_gl == GXM_COLOR_FORMAT_TO_GL_FORMAT.end()) {
        LOG_ERROR("Color format not implemented: {}, report this to developer", fmt::underlying(format));
        return;
    }

    std::uint8_t *temp_store = reinterpret_cast<std::uint8_t *>(pixels);
    std::vector<std::uint8_t> storage_v;

    if (format_need_temp_storage(renderer, surface, storage_v, width, height)) {
        temp_store = storage_v.data();
    }

    const SceGxmColorBaseFormat base_format = gxm::get_base_format(format);
    if (renderer.features.preserve_f16_nan_as_u16 && color::is_write_surface_stored_rawly(base_format)) {
        // we can't get the content of raw textures with glReadPixels
        GLint last_texture = 0;

        glGetIntegerv(GL_TEXTURE_BINDING_2D, &last_texture);
        glBindTexture(GL_TEXTURE_2D, context.current_color_attachment);
        glGetTexImage(GL_TEXTURE_2D, 0, color::get_raw_store_upload_format_type(base_format), color::get_raw_store_upload_data_type(base_format), temp_store);
        glBindTexture(GL_TEXTURE_2D, last_texture);
    } else {
        glReadPixels(0, 0, static_cast<GLsizei>(width), static_cast<GLsizei>(height), format_gl->second.first, format_gl->second.second, temp_store);
    }
    post_process_pixels_data(renderer, pixels, temp_store, width, height, surface.strideInPixels, surface);

    glPixelStorei(GL_PACK_ROW_LENGTH, 0);
}

void GLState::render_frame(const SceFVector2 &viewport_pos, const SceFVector2 &viewport_size, DisplayState &display,
    const GxmState &gxm, MemState &mem) {
    should_display = false;

    DisplayFrameInfo frame;
    {
        std::lock_guard<std::mutex> guard(display.display_info_mutex);
        frame = display.next_rendered_frame;
    }

    if (!frame.base)
        return;

    // Check if the surface exists
    float uvs[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    bool need_uv = true;

    const std::size_t texture_data_size = static_cast<size_t>(frame.pitch) * static_cast<size_t>(frame.image_size.y) * sizeof(uint32_t);

    SceFVector2 texture_size;

    uint64_t surface_handle = surface_cache.sourcing_color_surface_for_presentation(
        frame.base, frame.image_size.x, frame.image_size.y, frame.pitch, uvs, this->res_multiplier, texture_size);

    GLint last_texture = 0;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &last_texture);

    if (!surface_handle) {
        // Fallback to a manual upload (likely a black !!!)
        need_uv = false;
        glBindTexture(GL_TEXTURE_2D, screen_renderer.get_resident_texture());

        // Maybe a victim of surface locking (early from client GXM) when no frame yet renders!
        const auto pixels = frame.base.cast<void>().get(mem);

        if (pixels) {
            open_access_parent_protect_segment(mem, frame.base.address());
            unprotect_inner(mem, frame.base.address(), texture_data_size);
        }

        glPixelStorei(GL_UNPACK_ROW_LENGTH, frame.pitch);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, frame.image_size.x, frame.image_size.y, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);

        if (pixels) {
            close_access_parent_protect_segment(mem, frame.base.address());
        }

        texture_size.x = static_cast<float>(frame.image_size.x);
        texture_size.y = static_cast<float>(frame.image_size.y);

        surface_handle = screen_renderer.get_resident_texture();
    } else {
        const GLint standard_swizzle[4] = { GL_RED, GL_GREEN, GL_BLUE, GL_ALPHA };

        glBindTexture(GL_TEXTURE_2D, surface_handle);
        glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_RGBA, standard_swizzle);
    }

    glBindTexture(GL_TEXTURE_2D, last_texture);

    screen_renderer.render(viewport_pos, viewport_size, need_uv ? uvs : nullptr, static_cast<GLuint>(surface_handle), texture_size);
}

void GLState::swap_window(SDL_Window *window) {
    SDL_GL_SwapWindow(window);
}

std::vector<uint32_t> GLState::dump_frame(DisplayState &display, uint32_t &width, uint32_t &height) {
    DisplayFrameInfo frame;
    {
        std::lock_guard<std::mutex> guard(display.display_info_mutex);
        frame = display.next_rendered_frame;
    }

    width = static_cast<uint32_t>(frame.image_size.x * res_multiplier);
    height = static_cast<uint32_t>(frame.image_size.y * res_multiplier);
    return surface_cache.dump_frame(frame.base, width, height, frame.pitch, res_multiplier, features.support_get_texture_sub_image);
}

int GLState::get_supported_filters() {
    // actually it's not even bilinear, it's either bilinear or nearest depending on the last use of the texture..
    // TODO: add bicubic filter and allow disabling bilinear.
    return static_cast<int>(Filter::BILINEAR) | static_cast<int>(Filter::FXAA);
}

void GLState::set_screen_filter(const std::string_view &filter) {
    // we only support bilinear and fxaa
    screen_renderer.enable_fxaa = (filter == "FXAA");
}

int GLState::get_max_anisotropic_filtering() {
    float max_aniso;
    glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &max_aniso);
    return static_cast<int>(max_aniso);
}

void GLState::set_anisotropic_filtering(int anisotropic_filtering) {
    texture_cache.anisotropic_filtering = anisotropic_filtering;
}

int GLState::get_max_2d_texture_width() {
    GLint max_texture_size;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &max_texture_size);
    return static_cast<int>(max_texture_size);
}

std::string_view GLState::get_gpu_name() {
    return reinterpret_cast<const GLchar *>(glGetString(GL_RENDERER));
}

void GLState::precompile_shader(const ShadersHash &hash) {
    pre_compile_program(*this, hash);
}

void GLState::preclose_action() {}

} // namespace renderer::gl
