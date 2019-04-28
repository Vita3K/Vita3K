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

#include "app_functions.h"

#include "app_state.h"
#include "sfo.h"

#include <audio/functions.h>
#include <glutil/gl.h>
#include <host/version.h>
#include <io/functions.h>
#include <io/io.h> // vfs
#include <rtc/rtc.h>
#include <util/lock_and_find.h>
#include <util/log.h>

#include <SDL_filesystem.h>
#include <SDL_video.h>
#include <glbinding-aux/types_to_string.h>
#include <glbinding/Binding.h>
#include <microprofile.h>

#include <sstream>

#ifdef WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <dirent.h>
#include <util/string_utils.h>
#else
#include <sys/stat.h>
#include <unistd.h>
#endif

using namespace glbinding;

namespace app {
static constexpr auto DEFAULT_RES_WIDTH = 960;
static constexpr auto DEFAULT_RES_HEIGHT = 544;

#if MICROPROFILE_ENABLED
static void before_callback(const glbinding::FunctionCall &fn) {
    const MicroProfileToken token = MicroProfileGetToken("OpenGL", fn.function->name(), MP_CYAN, MicroProfileTokenTypeCpu);
    MICROPROFILE_ENTER_TOKEN(token);
}
#endif // MICROPROFILE_ENABLED

static void after_callback(const glbinding::FunctionCall &fn) {
    MICROPROFILE_LEAVE();
    for (GLenum error = glGetError(); error != GL_NO_ERROR; error = glGetError()) {
        std::stringstream gl_error;
        gl_error << error;
        LOG_ERROR("OpenGL: {} set error {}.", fn.function->name(), gl_error.str());
        assert(false);
    }
}

void update_viewport(State &app) {
    int w = 0;
    int h = 0;
    SDL_GL_GetDrawableSize(app.window.get(), &w, &h);

    HostState &host = app.host;
    host.drawable_size.x = w;
    host.drawable_size.y = h;

    if (h > 0) {
        const float window_aspect = static_cast<float>(w) / h;
        const float vita_aspect = static_cast<float>(DEFAULT_RES_WIDTH) / DEFAULT_RES_HEIGHT;
        if (window_aspect > vita_aspect) {
            // Window is wide. Pin top and bottom.
            host.viewport_size.x = h * vita_aspect;
            host.viewport_size.y = static_cast<SceFloat>(h);
            host.viewport_pos.x = (w - host.viewport_size.x) / 2;
            host.viewport_pos.y = 0;
        } else {
            // Window is tall. Pin left and right.
            host.viewport_size.x = static_cast<SceFloat>(w);
            host.viewport_size.y = w / vita_aspect;
            host.viewport_pos.x = 0;
            host.viewport_pos.y = (h - host.viewport_size.y) / 2;
        }
    } else {
        host.viewport_pos.x = 0;
        host.viewport_pos.y = 0;
        host.viewport_size.x = 0;
        host.viewport_size.y = 0;
    }
}

static bool init_host(HostState &state, const WindowPtr &window, Config cfg, const char *base_path, const char *pref_path) {
    const ResumeAudioThread resume_thread = [&state](SceUID thread_id) {
        const ThreadStatePtr thread = lock_and_find(thread_id, state.kernel.threads, state.kernel.mutex);
        const std::lock_guard<std::mutex> lock(thread->mutex);
        if (thread->to_do == ThreadToDo::wait) {
            thread->to_do = ThreadToDo::run;
        }
        thread->something_to_do.notify_all();
    };

    state.cfg = std::move(cfg);
    if (state.cfg.wait_for_debugger) {
        state.kernel.wait_for_debugger = state.cfg.wait_for_debugger.value();
    }
    state.base_path = base_path;

    // If configuration already provides preference path
    if (!state.cfg.pref_path) {
        state.pref_path = pref_path;
        state.cfg.pref_path = state.pref_path;
    } else {
        state.pref_path = state.cfg.pref_path.value();
    }

    state.gxm.window = window;
    if (!init(state.mem) || !init(state.audio, resume_thread) || !init(state.io, state.pref_path.c_str(), state.base_path.c_str())) {
        return false;
    }

    state.kernel.base_tick = { rtc_base_ticks() };

    std::string dir_path = state.pref_path + "ux0/app";
#ifdef WIN32
    _WDIR *d = _wopendir((string_utils::utf_to_wide(dir_path)).c_str());
    _wdirent *dp;
#else
    DIR *d = opendir(dir_path.c_str());
    dirent *dp;
#endif
    do {
        std::string d_name_utf8;
#ifdef WIN32
        if ((dp = _wreaddir(d)) != NULL) {
            d_name_utf8 = string_utils::wide_to_utf(dp->d_name);
#else
        if ((dp = readdir(d)) != NULL) {
            d_name_utf8 = dp->d_name;
#endif
            if ((strcmp(d_name_utf8.c_str(), ".")) && (strcmp(d_name_utf8.c_str(), ".."))) {
                vfs::FileBuffer params;
                state.io.title_id = d_name_utf8;
                if (vfs::read_app_file(params, state.pref_path, state.io.title_id, "sce_sys/param.sfo")) {
                    SfoFile sfo_handle;
                    load_sfo(sfo_handle, params);
                    find_data(state.game_version, sfo_handle, "APP_VER");
                    find_data(state.game_title, sfo_handle, "TITLE");
                    std::replace(state.game_title.begin(), state.game_title.end(), '\n', ' ');
                    state.gui.game_selector.games.push_back({ state.game_version, state.game_title, state.io.title_id });
                }
            }
        }
    } while (dp);

#ifdef WIN32
    _wclosedir(d);
#else
    closedir(d);
#endif

    return true;
}

bool init(State &app, Config cfg) {
    const std::unique_ptr<char, void (&)(void *)> base_path(SDL_GetBasePath(), SDL_free);
    const std::unique_ptr<char, void (&)(void *)> pref_path(SDL_GetPrefPath(org_name, app_name), SDL_free);

    app.window = WindowPtr(SDL_CreateWindow(window_title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, DEFAULT_RES_WIDTH, DEFAULT_RES_HEIGHT, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE), SDL_DestroyWindow);
    if (!app.window) {
        error_dialog("Could not create window", NULL);
        return false;
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

    app.gl_context = GLContextPtr(SDL_GL_CreateContext(app.window.get()), SDL_GL_DeleteContext);
    if (!app.gl_context) {
        error_dialog("Could not create OpenGL context!\nDoes your GPU support OpenGL 4.1?", NULL);
        return false;
    }

    // Try adaptive vsync first, falling back to regular vsync.
    if (SDL_GL_SetSwapInterval(-1) < 0) {
        SDL_GL_SetSwapInterval(1);
    }
    LOG_INFO("Swap interval = {}", SDL_GL_GetSwapInterval());

    const glbinding::GetProcAddress get_proc_address = [](const char *name) {
        return reinterpret_cast<ProcAddress>(SDL_GL_GetProcAddress(name));
    };
    Binding::initialize(get_proc_address, false);
    Binding::setCallbackMaskExcept(CallbackMask::Before | CallbackMask::After, { "glGetError" });
#if MICROPROFILE_ENABLED != 0
    Binding::setBeforeCallback(before_callback);
#endif // MICROPROFILE_ENABLED
    Binding::setAfterCallback(after_callback);

    if (!app.screen_renderer.init(base_path.get())) {
        error_dialog("Could not create screen renderer", app.window.get());
        return false;
    }

    if (!init_host(app.host, app.window, cfg, base_path.get(), pref_path.get())) {
        return false;
    }

    update_viewport(app);

    return true;
}
} // namespace app
