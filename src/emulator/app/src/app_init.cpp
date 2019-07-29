// Vita3K emulator project
// Copyright (C) 2018 Vita3K team
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

#include <app/functions.h>

#include <audio/functions.h>
#include <config/functions.h>
#include <config/version.h>
#include <glutil/gl.h>
#include <host/state.h>
#include <io/functions.h>
#include <renderer/functions.h>
#include <rtc/rtc.h>
#include <util/fs.h>
#include <util/lock_and_find.h>
#include <util/log.h>

#if DISCORD_RPC
#include <app/discord.h>
#endif

#ifdef USE_GDBSTUB
#include <gdbstub/functions.h>
#endif

#include <SDL_video.h>
#include <microprofile.h>

#include <sstream>

namespace app {

static constexpr auto DEFAULT_RES_WIDTH = 960;
static constexpr auto DEFAULT_RES_HEIGHT = 544;

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
        assert(false);
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

void update_viewport(HostState &state) {
    int w = 0;
    int h = 0;
    SDL_GL_GetDrawableSize(state.window.get(), &w, &h);

    state.drawable_size.x = w;
    state.drawable_size.y = h;

    if (h > 0) {
        const float window_aspect = static_cast<float>(w) / h;
        const float vita_aspect = static_cast<float>(DEFAULT_RES_WIDTH) / DEFAULT_RES_HEIGHT;
        if (window_aspect > vita_aspect) {
            // Window is wide. Pin top and bottom.
            state.viewport_size.x = h * vita_aspect;
            state.viewport_size.y = static_cast<SceFloat>(h);
            state.viewport_pos.x = (w - state.viewport_size.x) / 2;
            state.viewport_pos.y = 0;
        } else {
            // Window is tall. Pin left and right.
            state.viewport_size.x = static_cast<SceFloat>(w);
            state.viewport_size.y = w / vita_aspect;
            state.viewport_pos.x = 0;
            state.viewport_pos.y = (h - state.viewport_size.y) / 2;
        }
    } else {
        state.viewport_pos.x = 0;
        state.viewport_pos.y = 0;
        state.viewport_size.x = 0;
        state.viewport_size.y = 0;
    }
}

bool init(HostState &state, Config cfg, const Root &root_paths) {
    const ResumeAudioThread resume_thread = [&state](SceUID thread_id) {
        const auto thread = lock_and_find(thread_id, state.kernel.threads, state.kernel.mutex);
        const std::lock_guard<std::mutex> lock(thread->mutex);
        if (thread->to_do == ThreadToDo::wait) {
            thread->to_do = ThreadToDo::run;
        }
        thread->something_to_do.notify_all();
    };

    state.cfg = std::move(cfg);
    state.kernel.wait_for_debugger = state.cfg.wait_for_debugger;

    state.base_path = root_paths.get_base_path_string();

    // If configuration does not provide a preference path, use SDL's default
    if (state.cfg.pref_path == root_paths.get_pref_path_string() || state.cfg.pref_path.empty())
        state.pref_path = root_paths.get_pref_path_string() + '/';
    else
        state.pref_path = state.cfg.pref_path + '/';

    state.window = WindowPtr(SDL_CreateWindow(window_title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, DEFAULT_RES_WIDTH, DEFAULT_RES_HEIGHT, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE), SDL_DestroyWindow);
    if (!state.window || !init(state.mem) || !init(state.audio, resume_thread) || !init(state.io, state.base_path, state.pref_path)) {
        return false;
    }

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

        state.glcontext = GLContextPtr(SDL_GL_CreateContext(state.window.get()), SDL_GL_DeleteContext);
        if (state.glcontext) {
            choosen_minor_version = accept_gl_version[i];
            break;
        }
    }

    if (!state.glcontext) {
        error_dialog("Could not create OpenGL context!\nDoes your GPU at least support OpenGL 4.1?", NULL);
        return false;
    }

#if DISCORD_RPC
    discord::init();
    if (cfg.discord_rich_presence) {
        discord::update_presence("");
    }
#endif
    update_viewport(state);

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

    // Detect feature
    std::string version = reinterpret_cast<const GLchar *>(glGetString(GL_SHADING_LANGUAGE_VERSION));

    LOG_INFO("GL_VERSION = {}", glGetString(GL_VERSION));
    LOG_INFO("GL_SHADING_LANGUAGE_VERSION = {}", version);

    // Try to parse and get version
    const std::size_t dot_pos = version.find_first_of('.');

    if (dot_pos != std::string::npos) {
        const std::string major = version.substr(0, dot_pos);
        const std::string minor = version.substr(dot_pos + 1);

        state.features.direct_pack_unpack_half = false;

        if (std::atoi(major.c_str()) >= 4 && minor.length() >= 1) {
            if (minor[0] >= '2') {
                state.features.direct_pack_unpack_half = true;
            }
        }
    }

    if (choosen_minor_version >= 3) {
        glDebugMessageCallback(debug_output_callback, nullptr);
    }

    int total_extensions = 0;
    glGetIntegerv(GL_NUM_EXTENSIONS, &total_extensions);

    std::unordered_map<std::string, bool *> check_extensions = {
        { "GL_ARB_fragment_shader_interlock", &state.features.support_shader_interlock },
        { "GL_ARB_texture_barrier", &state.features.support_texture_barrier },
        { "GL_EXT_shader_framebuffer_fetch", &state.features.direct_fragcolor },
        { "GL_ARB_shading_language_packing", &state.features.pack_unpack_half_through_ext }
    };

    for (int i = 0; i < total_extensions; i++) {
        const std::string extension = reinterpret_cast<const GLchar *>(glGetStringi(GL_EXTENSIONS, i));
        auto find_result = check_extensions.find(extension);

        if (find_result != check_extensions.end()) {
            *find_result->second = true;
            check_extensions.erase(find_result);
        }
    }

    // Workaround driver bugs
    const std::string gpu_name = reinterpret_cast<const GLchar *>(glGetString(GL_RENDERER));
    const bool is_rtx = gpu_name.find("GeForce RTX") != std::string::npos;

    if (is_rtx) {
        // Disable shader interlock for all drivers.
        // TODO: Report and fix this on NVIDIA GeForce GTX driver
        state.features.support_shader_interlock = false;
    }

    if (state.features.direct_fragcolor) {
        LOG_INFO("Your GPU supports direct access to last fragment color. Your performance with programmable blending games will be optimized.");
    } else if (state.features.support_shader_interlock) {
        LOG_INFO("Your GPU supports shader interlock, some games that use programmable blending will have better performance.");
    } else if (state.features.support_texture_barrier) {
        LOG_INFO("Your GPU only supports texture barrier, performance may not be good on programmable blending games.");

        if (is_rtx)
            LOG_WARN("Shader interlock on GeForce RTX GPU driver is disabled due to a bug in the driver pending to be fixed.");
        else
            LOG_WARN("Consider updating to GPU that has shader interlock.");
    } else {
        LOG_INFO("Your GPU doesn't support extensions that make programmable blending possible. Some games may have broken graphics.");
        LOG_WARN("Consider updating your graphics drivers or upgrading your GPU.");
    }

    state.features.hardware_flip = state.cfg.hardware_flip;

    state.kernel.base_tick = { rtc_base_ticks() };

    if (state.cfg.overwrite_config)
        config::serialize_config(state.cfg, state.cfg.config_path);

    if (!renderer::init(state.renderer, renderer::Backend::OpenGL)) {
        return false;
    }

    return true;
}

void destory(HostState &host) {
#ifdef USE_DISCORD_RICH_PRESENCE
    discord::shutdown();
#endif

#ifdef USE_GDBSTUB
    server_close(host);
#endif

    // There may be changes that made in the GUI, so we should save, again
    if (host.cfg.overwrite_config)
        config::serialize_config(host.cfg, host.cfg.config_path);
}

} // namespace app
