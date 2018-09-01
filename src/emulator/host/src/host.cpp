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

#include <host/functions.h>

#include <host/app.h>
#include <host/import_fn.h>
#include <host/state.h>
#include <host/version.h>

#include <audio/functions.h>
#include <cpu/functions.h>
#include <glutil/gl.h>
#include <io/functions.h>
#include <kernel/functions.h>
#include <kernel/thread/thread_state.h>
#include <nids/functions.h>
#include <rtc/rtc.h>
#include <util/lock_and_find.h>
#include <util/log.h>

#include <SDL_events.h>
#include <SDL_filesystem.h>
#include <SDL_video.h>

#include <glbinding-aux/types_to_string.h>
#include <glbinding/Binding.h>
#include <microprofile.h>

#include <cassert>
#include <iomanip>
#include <iostream>
#include <sstream>

#include <gui/imgui_impl_sdl_gl3.h>

#ifdef WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <dirent.h>
#include <util/string_utils.h>
#else
#include <sys/stat.h>
#include <unistd.h>
#endif

#ifdef _WIN32
// Use discrete GPU by default

extern "C" {
// NVIDIA Optimus (Driver: 302+)
//     See: http://developer.download.nvidia.com/devzone/devcenter/gamegraphics/files/OptimusRenderingPolicies.pdf
__declspec(dllexport) unsigned long NvOptimusEnablement = 0x00000001;
// AMD (Driver: 13.35+)
//     See: https://gpuopen.com/amdpowerxpressrequesthighperformance/
__declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}
#endif

using namespace glbinding;

static constexpr bool LOG_IMPORT_CALLS = false;
static constexpr bool LOG_UNK_NIDS_ALWAYS = false;

#define NID(name, nid) extern const ImportFn import_##name;
#include <nids/nids.h>
#undef NID

static ImportFn resolve_import(uint32_t nid) {
    switch (nid) {
#define NID(name, nid) \
    case nid:          \
        return import_##name;
#include <nids/nids.h>
#undef NID
    }

    return ImportFn();
}

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

static void update_viewport(HostState &state) {
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
            state.viewport_size.y = h;
            state.viewport_pos.x = (w - state.viewport_size.x) / 2;
            state.viewport_pos.y = 0;
        } else {
            // Window is tall. Pin left and right.
            state.viewport_size.x = w;
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

static void handle_window_event(HostState &state, const SDL_WindowEvent event) {
    switch (static_cast<SDL_WindowEventID>(event.event)) {
    case SDL_WINDOWEVENT_SIZE_CHANGED:
        update_viewport(state);
        break;
    default:
        break;
    }
}

bool init(HostState &state) {
    const std::unique_ptr<char, void (&)(void *)> base_path(SDL_GetBasePath(), SDL_free);
    const std::unique_ptr<char, void (&)(void *)> pref_path(SDL_GetPrefPath(org_name, app_name), SDL_free);

    const ResumeAudioThread resume_thread = [&state](SceUID thread_id) {
        const ThreadStatePtr thread = lock_and_find(thread_id, state.kernel.threads, state.kernel.mutex);
        const std::lock_guard<std::mutex> lock(thread->mutex);
        if (thread->to_do == ThreadToDo::wait) {
            thread->to_do = ThreadToDo::run;
        }
        thread->something_to_do.notify_all();
    };

    state.base_path = base_path.get();
    state.pref_path = pref_path.get();
    state.window = WindowPtr(SDL_CreateWindow(window_title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, DEFAULT_RES_WIDTH, DEFAULT_RES_HEIGHT, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE), SDL_DestroyWindow);
    if (!state.window || !init(state.mem) || !init(state.audio, resume_thread) || !init(state.io, state.pref_path.c_str())) {
        return false;
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

    state.glcontext = GLContextPtr(SDL_GL_CreateContext(state.window.get()), SDL_GL_DeleteContext);
    if (!state.glcontext) {
        LOG_ERROR("Could not create OpenGL context.");
        return false;
    }

    update_viewport(state);

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
                Buffer params;
                state.io.title_id = d_name_utf8;
                if (read_file_from_disk(params, "sce_sys/param.sfo", state)) {
                    SfoFile sfo_handle;
                    load_sfo(sfo_handle, params);
                    find_data(state.game_title, sfo_handle, "TITLE");
                    state.gui.game_selector.games.push_back({ state.game_title, state.io.title_id });
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

bool handle_events(HostState &host) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        ImGui_ImplSdlGL3_ProcessEvent(&event);
        switch (event.type) {
        case SDL_QUIT:
            stop_all_threads(host.kernel);
            host.gxm.display_queue.abort();
            host.display.abort.exchange(true);
            host.display.condvar.notify_all();
            return false;

        case SDL_KEYDOWN:
            if (event.key.keysym.sym == SDLK_g) {
                auto &display = host.display;

                // toggle gui state
                bool old_imgui_render = display.imgui_render.load();
                while (!display.imgui_render.compare_exchange_weak(old_imgui_render, !old_imgui_render)) {
                }
            }

        case SDL_WINDOWEVENT:
            handle_window_event(host, event.window);
            break;
        }
    }

    return true;
}

/**
 * \brief Resolves a function imported from a loaded module.
 * \param kernel Kernel state struct
 * \param nid NID to resolve
 * \return Resolved address, 0 if not found
 */
Address resolve_export(KernelState &kernel, uint32_t nid) {
    const ExportNids::iterator export_address = kernel.export_nids.find(nid);
    if (export_address == kernel.export_nids.end()) {
        return 0;
    }

    return export_address->second;
}

void call_import(HostState &host, CPUState &cpu, uint32_t nid, SceUID thread_id) {
    Address export_pc = resolve_export(host.kernel, nid);

    if (!export_pc) {
        // HLE - call our C++ function

        if (LOG_IMPORT_CALLS) {
            const char *const name = import_name(nid);
            LOG_TRACE("THREAD_ID {} NID {} ({}) called", thread_id, log_hex(nid), name);
        }
        const ImportFn fn = resolve_import(nid);
        if (fn) {
            fn(host, cpu, thread_id);
        } else if (host.missing_nids.count(nid) == 0 || LOG_UNK_NIDS_ALWAYS) {
            const ThreadStatePtr thread = lock_and_find(thread_id, host.kernel.threads, host.kernel.mutex);
            LOG_ERROR("Import function for NID {} not found (thread name: {}, thread ID: {})", log_hex(nid), thread->name, thread_id);

            if (!LOG_UNK_NIDS_ALWAYS)
                host.missing_nids.insert(nid);
        }
    } else {
        // LLE - directly run ARM code imported from some loaded module

        if (LOG_IMPORT_CALLS) {
            const char *const name = import_name(nid);
            LOG_TRACE("THREAD_ID {} EXPORTED NID {} at {} ({})) called", thread_id, log_hex(nid), log_hex(export_pc), name);
        }
        const ThreadStatePtr thread = lock_and_find(thread_id, host.kernel.threads, host.kernel.mutex);
        const std::lock_guard<std::mutex> lock(thread->mutex);
        write_pc(*thread->cpu, export_pc);
    }
}
