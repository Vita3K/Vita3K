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
namespace texture {
bool init(GLTextureCacheState &cache) {
    cache.select_callback = [&](const std::size_t index) {
        const GLuint gl_texture = cache.textures[index];
        glBindTexture(GL_TEXTURE_2D, gl_texture);
    };

    cache.configure_texture_callback = [](const std::size_t index, const void *texture) {
        configure_bound_texture(*reinterpret_cast<const SceGxmTexture *>(texture));
    };

    cache.upload_texture_callback = [](const std::size_t index, const void *texture, const MemState &mem) {
        upload_bound_texture(*reinterpret_cast<const SceGxmTexture *>(texture), mem);
    };

    return cache.textures.init(reinterpret_cast<renderer::Generator *>(glGenTextures), reinterpret_cast<renderer::Deleter *>(glDeleteTextures));
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

void bind_fundamental(GLContext &context) {
    // Bind the vertex array and element buffer.
    glBindVertexArray(context.vertex_array[0]);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, context.element_buffer[0]);
}

#if MICROPROFILE_ENABLED
static void before_callback(const char *name, void *funcptr, int len_args, ...) {
    const MicroProfileToken token = MicroProfileGetToken("OpenGL", name, MP_CYAN, MicroProfileTokenTypeCpu);
    MICROPROFILE_ENTER_TOKEN(token);
}
#endif // MICROPROFILE_ENABLED

static void after_callback(const char *name, void *funcptr, int len_args, ...) {
    MICROPROFILE_LEAVE();
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

bool create(SDL_Window *window, std::unique_ptr<State> &state) {
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

    // Try to parse and get version
    const std::size_t dot_pos = version.find_first_of('.');

    if (dot_pos != std::string::npos) {
        const std::string major = version.substr(0, dot_pos);
        const std::string minor = version.substr(dot_pos + 1);

        gl_state.features.direct_pack_unpack_half = false;
        gl_state.features.use_shader_binding = false;

        if (std::atoi(major.c_str()) >= 4 && minor.length() >= 1) {
            if (minor[0] >= '2') {
                gl_state.features.direct_pack_unpack_half = true;
                gl_state.features.use_shader_binding = true;
            }
        }
    }

    if (choosen_minor_version >= 3) {
        glDebugMessageCallback(reinterpret_cast<GLDEBUGPROC>(debug_output_callback), nullptr);
    }

    int total_extensions = 0;
    glGetIntegerv(GL_NUM_EXTENSIONS, &total_extensions);

    std::unordered_map<std::string, bool *> check_extensions = {
        { "GL_ARB_fragment_shader_interlock", &gl_state.features.support_shader_interlock },
        { "GL_ARB_texture_barrier", &gl_state.features.support_texture_barrier },
        { "GL_EXT_shader_framebuffer_fetch", &gl_state.features.direct_fragcolor },
        { "GL_ARB_shading_language_packing", &gl_state.features.pack_unpack_half_through_ext }
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

    gl_state.features.use_ubo = true;

    return true;
}

bool create(std::unique_ptr<Context> &context) {
    R_PROFILE(__func__);

    context = std::make_unique<GLContext>();
    GLContext *gl_context = reinterpret_cast<GLContext *>(context.get());

    return !(!texture::init(gl_context->texture_cache) || !gl_context->vertex_array.init(reinterpret_cast<renderer::Generator *>(glGenVertexArrays), reinterpret_cast<renderer::Deleter *>(glDeleteVertexArrays)) || !gl_context->element_buffer.init(reinterpret_cast<renderer::Generator *>(glGenBuffers), reinterpret_cast<renderer::Deleter *>(glDeleteBuffers)) || !gl_context->stream_vertex_buffers.init(reinterpret_cast<renderer::Generator *>(glGenBuffers), reinterpret_cast<renderer::Deleter *>(glDeleteBuffers))
        || !gl_context->uniform_buffer.init(reinterpret_cast<renderer::Generator *>(glGenBuffers), reinterpret_cast<renderer::Deleter *>(glDeleteBuffers)));
}

bool create(std::unique_ptr<RenderTarget> &rt, const SceGxmRenderTargetParams &params, const FeatureState &features) {
    R_PROFILE(__func__);

    rt = std::make_unique<GLRenderTarget>();
    GLRenderTarget *render_target = reinterpret_cast<GLRenderTarget *>(rt.get());
    if (!render_target->renderbuffers.init(reinterpret_cast<renderer::Generator *>(glGenRenderbuffers), reinterpret_cast<renderer::Deleter *>(glDeleteRenderbuffers)) || !render_target->framebuffer.init(reinterpret_cast<renderer::Generator *>(glGenFramebuffers), reinterpret_cast<renderer::Deleter *>(glDeleteFramebuffers))) {
        return false;
    }

    if (!render_target->maskbuffer.init(reinterpret_cast<renderer::Generator *>(glGenFramebuffers), reinterpret_cast<renderer::Deleter *>(glDeleteFramebuffers))) {
        return false;
    }

    render_target->width = params.width;
    render_target->height = params.height;

    int depth_fb_index = 1;

    if (features.is_programmable_blending_need_to_bind_color_attachment()) {
        depth_fb_index = 0;
        render_target->color_attachment.init(reinterpret_cast<renderer::Generator *>(glGenTextures), reinterpret_cast<renderer::Deleter *>(glDeleteTextures));

        glBindTexture(GL_TEXTURE_2D, render_target->color_attachment[0]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, params.width, params.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    } else {
        glBindRenderbuffer(GL_RENDERBUFFER, render_target->renderbuffers[0]);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA, params.width, params.height);
    }

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
    glClearDepth(1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    return true;
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
        frag_program_gl->blend_enabled = true;
        frag_program_gl->color_func = translate_blend_func(blend->colorFunc);
        frag_program_gl->alpha_func = translate_blend_func(blend->alphaFunc);
        frag_program_gl->color_src = translate_blend_factor(blend->colorSrc);
        frag_program_gl->color_dst = translate_blend_factor(blend->colorDst);
        frag_program_gl->alpha_src = translate_blend_factor(blend->alphaSrc);
        frag_program_gl->alpha_dst = translate_blend_factor(blend->alphaDst);
    }
    shader::usse::get_uniform_buffer_sizes(program, fp->uniform_buffer_sizes);

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

    vert_program_gl->attribute_locations = attribute_locations(program);

    return true;
}

void set_context(GLContext &context, GxmContextState &state, const MemState &mem, const GLRenderTarget *rt, const FeatureState &features) {
    R_PROFILE(__func__);

    bind_fundamental(context);

    if (rt) {
        context.render_target = rt;
    } else {
        context.render_target = reinterpret_cast<const GLRenderTarget *>(context.current_render_target);
    }

    if (state.fragment_program && state.fragment_program.get(mem)->is_maskupdate) {
        uint8_t mask = state.writing_mask ? 0xFF : 0;
        set_uniform_buffer(context, false, 14, sizeof(mask), &mask, false);

        glBindFramebuffer(GL_FRAMEBUFFER, context.render_target->maskbuffer[0]);
    } else {
        glBindFramebuffer(GL_FRAMEBUFFER, context.render_target->framebuffer[0]);
    }

    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glClearDepth(state.depth_stencil_surface.backgroundDepth);
    glClear(GL_DEPTH_BUFFER_BIT);
    glDisable(GL_DEPTH_TEST);

    sync_mask(context, state, mem);
    // TODO: Take request to force load from given memory

    // Sync enable/disable depth/stencil based on depth stencil surface.
    if (sync_depth_data(state)) {
        sync_front_depth_func(state);
        sync_front_depth_write_enable(state);
    }

    if (sync_stencil_data(state, mem)) {
        sync_stencil_func(state, mem, true);
        sync_stencil_func(state, mem, false);
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

void get_surface_data(GLContext &context, size_t width, size_t height, size_t stride_in_pixels, uint32_t *pixels, SceGxmColorFormat format, const bool do_flip) {
    R_PROFILE(__func__);

    if (pixels == nullptr) {
        return;
    }

    glPixelStorei(GL_PACK_ROW_LENGTH, static_cast<GLint>(stride_in_pixels));

    // TODO Need more check into this
    switch (format) {
    case SCE_GXM_COLOR_FORMAT_U8U8U8U8_ABGR:
    case SCE_GXM_COLOR_FORMAT_U8U8U8U8_ARGB:
        glReadPixels(0, 0, static_cast<GLsizei>(width), static_cast<GLsizei>(height), GL_RGBA, GL_UNSIGNED_BYTE, pixels);
        break;
    case SCE_GXM_COLOR_FORMAT_U8U8U8U8_RGBA:
        glReadPixels(0, 0, static_cast<GLsizei>(width), static_cast<GLsizei>(height), GL_RGBA, GL_UNSIGNED_BYTE, pixels);
        for (int i = 0; i < width * height; ++i) {
            uint8_t *pixel = reinterpret_cast<uint8_t *>(&pixels[i]);
            std::swap(pixel[0], pixel[3]);
            std::swap(pixel[1], pixel[2]);
        }
        break;
    case SCE_GXM_COLOR_FORMAT_U8U8U8_BGR:
        glReadPixels(0, 0, static_cast<GLsizei>(width), static_cast<GLsizei>(height), GL_RGB, GL_UNSIGNED_BYTE, pixels);
        break;
    case SCE_GXM_COLOR_FORMAT_U8_R:
        glReadPixels(0, 0, static_cast<GLsizei>(width), static_cast<GLsizei>(height), GL_RED, GL_UNSIGNED_BYTE, pixels);
        break;
    default:
        LOG_ERROR("Color format not implemented: {}, report this to developer", format);
        glReadPixels(0, 0, static_cast<GLsizei>(width), static_cast<GLsizei>(height), GL_RGBA, GL_UNSIGNED_BYTE, pixels);
        break;
    }

    glPixelStorei(GL_PACK_ROW_LENGTH, 0);

    if (do_flip) {
        flip_vertically(pixels, width, height, stride_in_pixels);
    }

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
