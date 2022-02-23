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
#include <renderer/state.h>
#include <renderer/types.h>

#include <renderer/gl/functions.h>
#include <renderer/gl/types.h>

#include <shader/spirv_recompiler.h>
#include <shader/usse_program_analyzer.h>

#include <features/state.h>

#include <gxm/functions.h>
#include <gxm/types.h>
#include <util/log.h>

#include <SDL.h>
#include <SDL_video.h>

#include <cassert>
#include <sstream>

namespace renderer::gl {

GLContext::GLContext()
    : vertex_stream_ring_buffer(GL_ARRAY_BUFFER, MB(128))
    , index_stream_ring_buffer(GL_ELEMENT_ARRAY_BUFFER, MB(64))
    , vertex_uniform_stream_ring_buffer(GL_SHADER_STORAGE_BUFFER, MB(256))
    , fragment_uniform_stream_ring_buffer(GL_SHADER_STORAGE_BUFFER, MB(256))
    , vertex_info_uniform_buffer(GL_UNIFORM_BUFFER, MB(8))
    , fragment_info_uniform_buffer(GL_UNIFORM_BUFFER, MB(8)) {
    std::memset(&previous_vert_info, 0, sizeof(GXMRenderVertUniformBlock));
    std::memset(&previous_frag_info, 0, sizeof(GXMRenderFragUniformBlock));
}

namespace texture {
bool init(GLTextureCacheState &cache, const bool hashless_texture_cache) {
    cache.select_callback = [&](const std::size_t index, const void *texture) {
        const SceGxmTexture *texture_casted = reinterpret_cast<const SceGxmTexture *>(texture);

        const GLuint gl_texture = cache.textures[index];
        glBindTexture(get_gl_texture_type(*texture_casted), gl_texture);
    };

    cache.configure_texture_callback = [](const std::size_t index, const void *texture) {
        configure_bound_texture(*reinterpret_cast<const SceGxmTexture *>(texture));
    };

    cache.upload_texture_callback = [](const std::size_t index, const void *texture, const MemState &mem) {
        upload_bound_texture(*reinterpret_cast<const SceGxmTexture *>(texture), mem);
    };

    cache.use_protect = hashless_texture_cache;

    return cache.textures.init(reinterpret_cast<renderer::Generator *>(glGenTextures), reinterpret_cast<renderer::Deleter *>(glDeleteTextures));
}
} // namespace texture

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

#if MICROPROFILE_ENABLED
static void before_callback(const char *name, void *funcptr, int len_args, ...) {
    const MicroProfileToken token = MicroProfileGetToken("OpenGL", name, MP_CYAN, MicroProfileTokenTypeCpu);
    MICROPROFILE_ENTER_TOKEN(token);
}
#endif // MICROPROFILE_ENABLED

static void after_callback(const char *name, void *funcptr, int len_args, ...) {
    // MICROPROFILE_LEAVE();
    for (GLenum error = glad_glGetError(); error != GL_NO_ERROR; error = glad_glGetError()) {
#ifndef NDEBUG
        std::stringstream gl_error;
        gl_error << error;
        LOG_ERROR("OpenGL: {} set error {}.", name, gl_error.str());
#else
        LOG_ERROR("OpenGL error: {}", log_hex(static_cast<std::uint32_t>(error)));
#endif
    }
}

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

bool create(SDL_Window *window, std::unique_ptr<State> &state, const bool hashless_texture_cache) {
    auto &gl_state = dynamic_cast<GLState &>(*state);

    // Recursively create GL version until one accepts
    // Major 4 is mandantory
    const int accept_gl_version[] = {
        5, // OpenGL 4.5
        3, // OpenGL 4.3
        2, // OpenGL 4.2
        1 // OpenGL 4.1
    };

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

    int choosen_minor_version = 0;

    for (int i = 0; i < sizeof(accept_gl_version) / sizeof(int); i++) {
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, accept_gl_version[i]);

        gl_state.context = GLContextPtr(SDL_GL_CreateContext(window), SDL_GL_DeleteContext);
        if (gl_state.context) {
            choosen_minor_version = accept_gl_version[i];
            break;
        }
    }

    if (!gl_state.context)
        return false;

    // Try adaptive vsync first, falling back to regular vsync.
    if (SDL_GL_SetSwapInterval(-1) < 0) {
        SDL_GL_SetSwapInterval(1);
    }
    LOG_INFO("Swap interval = {}", SDL_GL_GetSwapInterval());

    gladLoadGLLoader((GLADloadproc)SDL_GL_GetProcAddress);
#if MICROPROFILE_ENABLED != 0
    glad_set_pre_callback(before_callback);
#endif // MICROPROFILE_ENABLED
    glad_set_post_callback(after_callback);

    // Detect GPU and features
    const std::string gpu_name = reinterpret_cast<const GLchar *>(glGetString(GL_RENDERER));
    const std::string version = reinterpret_cast<const GLchar *>(glGetString(GL_SHADING_LANGUAGE_VERSION));

    LOG_INFO("GPU = {}", gpu_name);
    LOG_INFO("GL_VERSION = {}", glGetString(GL_VERSION));
    LOG_INFO("GL_SHADING_LANGUAGE_VERSION = {}", version);

    if (choosen_minor_version >= 3) {
        glDebugMessageCallback(reinterpret_cast<GLDEBUGPROC>(debug_output_callback), nullptr);
    }

    int total_extensions = 0;
    glGetIntegerv(GL_NUM_EXTENSIONS, &total_extensions);

    std::unordered_map<std::string, bool *> check_extensions = {
        { "GL_ARB_fragment_shader_interlock", &gl_state.features.support_shader_interlock },
        { "GL_ARB_texture_barrier", &gl_state.features.support_texture_barrier },
        { "GL_EXT_shader_framebuffer_fetch", &gl_state.features.direct_fragcolor },
        { "GL_ARB_gl_spirv", &gl_state.features.spirv_shader }
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
        LOG_WARN("Consider updating to GPU that has shader interlock.");
    } else {
        LOG_INFO("Your GPU doesn't support extensions that make programmable blending possible. Some games may have broken graphics.");
        LOG_WARN("Consider updating your graphics drivers or upgrading your GPU.");
    }

    return gl_state.init(hashless_texture_cache);
}

bool GLState::init(const bool hashless_texture_cache) {
    if (!texture::init(texture_cache, hashless_texture_cache)) {
        LOG_ERROR("Failed to initialize texture cache!");
        return false;
    }

    return true;
}

bool create(std::unique_ptr<Context> &context) {
    R_PROFILE(__func__);

    context = std::make_unique<GLContext>();
    GLContext *gl_context = reinterpret_cast<GLContext *>(context.get());

    const bool init_result = gl_context->vertex_array.init(reinterpret_cast<renderer::Generator *>(glGenVertexArrays), reinterpret_cast<renderer::Deleter *>(glDeleteVertexArrays));

    if (!init_result) {
        return false;
    }

    return true;
}

bool create(std::unique_ptr<RenderTarget> &rt, const SceGxmRenderTargetParams &params, const FeatureState &features) {
    R_PROFILE(__func__);

    rt = std::make_unique<GLRenderTarget>();
    GLRenderTarget *render_target = reinterpret_cast<GLRenderTarget *>(rt.get());

    if (!render_target->maskbuffer.init(reinterpret_cast<renderer::Generator *>(glGenFramebuffers), reinterpret_cast<renderer::Deleter *>(glDeleteFramebuffers))) {
        return false;
    }

    render_target->width = params.width;
    render_target->height = params.height;

    render_target->attachments.init(reinterpret_cast<renderer::Generator *>(glGenTextures), reinterpret_cast<renderer::Deleter *>(glDeleteTextures));

    glBindTexture(GL_TEXTURE_2D, render_target->attachments[0]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, params.width, params.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glBindTexture(GL_TEXTURE_2D, render_target->attachments[1]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH24_STENCIL8, params.width, params.height, 0, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, nullptr);

    render_target->masktexture.init(reinterpret_cast<renderer::Generator *>(glGenTextures), reinterpret_cast<renderer::Deleter *>(glDeleteTextures));
    glBindTexture(GL_TEXTURE_2D, render_target->masktexture[0]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, params.width, params.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindFramebuffer(GL_FRAMEBUFFER, render_target->maskbuffer[0]);
    glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, render_target->masktexture[0], 0);
    GLenum drawbuffers[1] = { GL_COLOR_ATTACHMENT0 };
    glDrawBuffers(1, drawbuffers);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    return true;
}

static void layout_ssbo_offset_from_uniform_buffer_sizes(UniformBufferSizes &sizes, UniformBufferSizes &offsets, std::size_t &total_hold) {
    std::uint32_t last_offset = 0;

    for (std::size_t i = 0; i < sizes.size(); i++) {
        if (sizes[i] != 0) {
            // Round to vec4 unit
            offsets[i] = last_offset;
            last_offset += ((sizes[i] + 3) / 4 * 4);
        } else {
            offsets[i] = static_cast<std::uint32_t>(-1);
        }
    }

    total_hold = static_cast<std::size_t>(last_offset);
}

bool create(std::unique_ptr<FragmentProgram> &fp, GLState &state, const SceGxmProgram &program, const SceGxmBlendInfo *blend, GXPPtrMap &gxp_ptr_map, const char *base_path, const char *title_id) {
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
        frag_program_gl->blend_enabled = (blend->colorFunc != SCE_GXM_BLEND_FUNC_NONE) || (blend->alphaFunc != SCE_GXM_BLEND_FUNC_NONE);
        frag_program_gl->color_func = translate_blend_func(blend->colorFunc);
        frag_program_gl->color_src = translate_blend_factor(blend->colorSrc);
        frag_program_gl->color_dst = translate_blend_factor(blend->colorDst);
        frag_program_gl->alpha_func = translate_blend_func(blend->alphaFunc);
        frag_program_gl->alpha_src = translate_blend_factor(blend->alphaSrc);
        frag_program_gl->alpha_dst = translate_blend_factor(blend->alphaDst);
    }
    shader::usse::get_uniform_buffer_sizes(program, fp->uniform_buffer_sizes);
    layout_ssbo_offset_from_uniform_buffer_sizes(fp->uniform_buffer_sizes, fp->uniform_buffer_data_offsets, fp->max_total_uniform_buffer_storage);

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

    shader::usse::get_uniform_buffer_sizes(program, vp->uniform_buffer_sizes);
    shader::usse::get_attribute_informations(program, vert_program_gl->attribute_infos);
    layout_ssbo_offset_from_uniform_buffer_sizes(vp->uniform_buffer_sizes, vp->uniform_buffer_data_offsets, vp->max_total_uniform_buffer_storage);

    if (vert_program_gl->attribute_infos.empty()) {
        vert_program_gl->stripped_symbols_checked = false;
    } else {
        vert_program_gl->stripped_symbols_checked = true;
    }

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
    if ((ds_surface_fin->depthData.address() == 0) && (ds_surface_fin->stencilData.address() == 0)) {
        ds_surface_fin = nullptr;
    }

    std::uint64_t current_color_attachment_handle = 0;
    std::uint16_t current_framebuffer_height = 0;

    context.current_framebuffer = static_cast<GLuint>(state.surface_cache.retrieve_framebuffer_handle(
        color_surface_fin, ds_surface_fin, &current_color_attachment_handle, nullptr, &current_framebuffer_height));
    context.current_color_attachment = static_cast<GLuint>(current_color_attachment_handle);
    context.current_framebuffer_height = current_framebuffer_height;

    glBindFramebuffer(GL_FRAMEBUFFER, context.current_framebuffer);

    if (context.record.region_clip_mode != SCE_GXM_REGION_CLIP_NONE) {
        glDisable(GL_SCISSOR_TEST);
    }

    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glClearDepth(context.record.depth_stencil_surface.backgroundDepth);
    glClear(GL_DEPTH_BUFFER_BIT);
    glDisable(GL_DEPTH_TEST);

    sync_mask(context, mem);
    // TODO: Take request to force load from given memory

    // Sync enable/disable depth/stencil based on depth stencil surface.
    if (sync_depth_data(context.record)) {
        sync_depth_func(context.record.front_depth_func, true);
        sync_depth_func(context.record.back_depth_func, false);
        sync_depth_write_enable(context.record.front_depth_write_mode, true);
        sync_depth_write_enable(context.record.back_depth_write_mode, false);
    }

    if (sync_stencil_data(context.record, mem)) {
        sync_stencil_func(context.record.back_stencil_state, mem, true);
        sync_stencil_func(context.record.front_stencil_state, mem, false);
    }

    if (context.record.region_clip_mode != SCE_GXM_REGION_CLIP_NONE) {
        glEnable(GL_SCISSOR_TEST);
    }
}

void get_surface_data(GLState &renderer, GLContext &context, size_t width, size_t height, size_t stride_in_pixels, uint32_t *pixels, SceGxmColorFormat format) {
    R_PROFILE(__func__);

    if (!pixels)
        return;

    glPixelStorei(GL_PACK_ROW_LENGTH, static_cast<GLint>(stride_in_pixels));

    // TODO Need more check into this
    switch (format) {
    case SCE_GXM_COLOR_FORMAT_U8U8U8U8_ABGR:
        glReadPixels(0, 0, static_cast<GLsizei>(width), static_cast<GLsizei>(height), GL_RGBA, GL_UNSIGNED_INT_8_8_8_8_REV, pixels);
        break;
    case SCE_GXM_COLOR_FORMAT_U8U8U8U8_ARGB:
        glReadPixels(0, 0, static_cast<GLsizei>(width), static_cast<GLsizei>(height), GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, pixels);
        break;
    case SCE_GXM_COLOR_FORMAT_U8U8U8U8_RGBA:
        glReadPixels(0, 0, static_cast<GLsizei>(width), static_cast<GLsizei>(height), GL_RGBA, GL_UNSIGNED_BYTE, pixels);
        for (int i = 0; i < width * height; ++i) {
            uint8_t *pixel = reinterpret_cast<uint8_t *>(&pixels[i]);
            std::swap(pixel[0], pixel[3]);
            std::swap(pixel[1], pixel[2]);
        }
        break;
    case SCE_GXM_COLOR_FORMAT_U4U4U4U4_ARGB:
        glReadPixels(0, 0, static_cast<GLsizei>(width), static_cast<GLsizei>(height), GL_BGRA, GL_UNSIGNED_SHORT_4_4_4_4_REV, pixels);
        break;
    case SCE_GXM_COLOR_FORMAT_U8U8U8_BGR:
        glReadPixels(0, 0, static_cast<GLsizei>(width), static_cast<GLsizei>(height), GL_RGB, GL_UNSIGNED_BYTE, pixels);
        break;
    case SCE_GXM_COLOR_FORMAT_U5U6U5_RGB:
        glReadPixels(0, 0, static_cast<GLsizei>(width), static_cast<GLsizei>(height), GL_RGB, GL_UNSIGNED_SHORT_5_6_5, pixels);
        break;
    case SCE_GXM_COLOR_FORMAT_U16_R:
        glReadPixels(0, 0, static_cast<GLsizei>(width), static_cast<GLsizei>(height), GL_RED, GL_UNSIGNED_SHORT, pixels);
        break;
    case SCE_GXM_COLOR_FORMAT_U8U8_AR:
        glReadPixels(0, 0, static_cast<GLsizei>(width), static_cast<GLsizei>(height), GL_RG, GL_UNSIGNED_BYTE, pixels);
        break;
    case SCE_GXM_COLOR_FORMAT_U8_A:
        glReadPixels(0, 0, static_cast<GLsizei>(width), static_cast<GLsizei>(height), GL_ALPHA, GL_UNSIGNED_BYTE, pixels);
        break;
    case SCE_GXM_COLOR_FORMAT_U8_R:
        glReadPixels(0, 0, static_cast<GLsizei>(width), static_cast<GLsizei>(height), GL_RED, GL_UNSIGNED_BYTE, pixels);
        break;
    case SCE_GXM_COLOR_FORMAT_U2F10F10F10_ABGR:
    case SCE_GXM_COLOR_FORMAT_U2U10U10U10_ABGR:
        glReadPixels(0, 0, static_cast<GLsizei>(width), static_cast<GLsizei>(height), GL_RGBA, GL_UNSIGNED_INT_2_10_10_10_REV, pixels);
        break;
    case SCE_GXM_COLOR_FORMAT_U10U10U10U2_RGBA:
        glReadPixels(0, 0, static_cast<GLsizei>(width), static_cast<GLsizei>(height), GL_RGBA, GL_UNSIGNED_INT_10_10_10_2, pixels);
        break;
    case SCE_GXM_COLOR_FORMAT_U10U10U10U2_BGRA:
        glReadPixels(0, 0, static_cast<GLsizei>(width), static_cast<GLsizei>(height), GL_BGRA, GL_UNSIGNED_INT_10_10_10_2, pixels);
        break;
    case SCE_GXM_COLOR_FORMAT_F16_R:
        glReadPixels(0, 0, static_cast<GLsizei>(width), static_cast<GLsizei>(height), GL_RED, GL_HALF_FLOAT, pixels);
        break;
    case SCE_GXM_COLOR_FORMAT_F16F16_GR:
        glReadPixels(0, 0, static_cast<GLsizei>(width), static_cast<GLsizei>(height), GL_RG, GL_HALF_FLOAT, pixels);
        break;
    case SCE_GXM_COLOR_FORMAT_F16F16F16F16_ABGR:
        glReadPixels(0, 0, static_cast<GLsizei>(width), static_cast<GLsizei>(height), GL_RGBA, GL_HALF_FLOAT, pixels);
        break;
    case SCE_GXM_COLOR_FORMAT_F16F16F16F16_ARGB:
        glReadPixels(0, 0, static_cast<GLsizei>(width), static_cast<GLsizei>(height), GL_BGRA, GL_HALF_FLOAT, pixels);
        break;
    case SCE_GXM_COLOR_FORMAT_F32_R:
        glReadPixels(0, 0, static_cast<GLsizei>(width), static_cast<GLsizei>(height), GL_RED, GL_FLOAT, pixels);
        break;
    case SCE_GXM_COLOR_FORMAT_F32F32_GR:
        glReadPixels(0, 0, static_cast<GLsizei>(width), static_cast<GLsizei>(height), GL_RG, GL_FLOAT, pixels);
        break;
    case SCE_GXM_COLOR_FORMAT_SE5M9M9M9_RGB:
    case SCE_GXM_COLOR_FORMAT_SE5M9M9M9_BGR: {
        std::vector<uint16_t> temp_bytes(width * height * 3);
        GLenum rfmt = (format == SCE_GXM_COLOR_FORMAT_SE5M9M9M9_BGR) ? GL_BGR : GL_RGB;
        glReadPixels(0, 0, static_cast<GLsizei>(width), static_cast<GLsizei>(height), rfmt, GL_HALF_FLOAT, temp_bytes.data());
        for (int i = 0, iptr = 0; i < width * height; ++i) {
            uint32_t pixel = 0;
            pixel |= (uint32_t(temp_bytes[iptr++] << 17) & (0x3FFFF << 18)); // Exp + 9 bits
            pixel |= (uint32_t(temp_bytes[iptr++] << 8) & (0x1FF << 9));
            pixel |= (uint32_t(temp_bytes[iptr++] >> 1) & (0x1FF << 0));
            pixels[i] = pixel;
        }
        break;
    }
    default:
        LOG_ERROR("Color format not implemented: {}, report this to developer", format);
        glReadPixels(0, 0, static_cast<GLsizei>(width), static_cast<GLsizei>(height), GL_RGBA, GL_UNSIGNED_BYTE, pixels);
        break;
    }

    if (context.record.color_surface.surfaceType == SCE_GXM_COLOR_SURFACE_TILED) {
        const SceGxmColorBaseFormat base_format = gxm::get_base_format(format);
        const size_t bpp = renderer::color::bits_per_pixel(base_format);
        const size_t bytes_per_pixel = (bpp + 7) >> 3;
        std::vector<uint8_t> buffer;

        buffer.resize(((width + 31) / 32) * ((height + 31) / 32) * 1024 * bytes_per_pixel);
        for (int j = 0; j < height; j++) {
            for (int hori_tile = 0; hori_tile < (width >> 5); hori_tile++) {
                const size_t tile_position = hori_tile + (j >> 5) * ((width + 31) >> 5);
                const size_t first_pixel_offset_in_tile = (tile_position << 10) + (j & 31) * 32;
                const size_t first_pixel_offset_in_linear = (j * stride_in_pixels) + hori_tile * 32;

                memcpy(buffer.data() + first_pixel_offset_in_tile * bytes_per_pixel,
                    (const char *)(pixels) + first_pixel_offset_in_linear * bytes_per_pixel, 32 * bytes_per_pixel);
            }
        }
        memcpy(pixels, buffer.data(), buffer.size());
    }

    glPixelStorei(GL_PACK_ROW_LENGTH, 0);
    ++renderer.texture_cache.timestamp;
}

} // namespace renderer::gl
