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

#include <host/app.h>
#include <host/functions.h>
#include <host/state.h>
#include <host/version.h>
#include <kernel/thread/thread_functions.h>
#include <kernel/thread/thread_state.h>
#include <util/find.h>
#include <util/log.h>
#include <util/resource.h>
#include <util/string_convert.h>

#include <SDL.h>
#include <glutil/gl.h>

#include <algorithm> // find_if_not
#include <cassert>
#include <iostream>
#include <sstream>

#include <gui/functions.h>
#include <gui/imgui_impl.h>

typedef std::unique_ptr<const void, void (*)(const void *)> SDLPtr;
typedef std::unique_ptr<SDL_Surface, void (*)(SDL_Surface *)> SurfacePtr;

enum ExitCode {
    Success = 0,
    IncorrectArgs,
    SDLInitFailed,
    HostInitFailed,
    ModuleLoadFailed,
    InitThreadFailed,
    RunThreadFailed
};

static bool is_macos_process_arg(const char *arg) {
    return strncmp(arg, "-psn_", 5) == 0;
}

static void error(const std::string &message, SDL_Window *window = nullptr) {
    if (SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", message.c_str(), window) < 0) {
        LOG_ERROR("SDL Error: {}", message);
    }
}

static void term_sdl(const void *succeeded) {
    assert(succeeded != nullptr);

    SDL_Quit();
}

int main(int argc, char *argv[]) {
    init_logging();

    LOG_INFO("{}", window_title);

    ProgramArgsWide argv_wide = process_args(argc, argv);

    const SDLPtr sdl(reinterpret_cast<const void *>(SDL_Init(SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER | SDL_INIT_VIDEO) >= 0), term_sdl);
    if (!sdl) {
        error("SDL initialisation failed.");
        return SDLInitFailed;
    }

    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

    const char *const *const path_arg = std::find_if_not(&argv[1], &argv[argc], is_macos_process_arg);
    std::wstring path;
    if (path_arg != &argv[argc]) {
        path = utf_to_wide(*path_arg);
    } else {
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_DROPFILE) {
                path = utf_to_wide(ev.drop.file);
                SDL_free(ev.drop.file);
                break;
            }
        }
    }

    HostState host;
    if (!init(host)) {
        error_dialog("Host initialisation failed.", host.window.get());
        return HostInitFailed;
    }

    imgui::init(host.window);

    bool is_vpk = true;

    while (path.empty() && handle_events(host) && is_vpk) {
        imgui::draw_begin(host.window);

        DrawUI(host);
        DrawGameSelector(host, &is_vpk);

        imgui::draw_end(host.window);
    }

    if (!is_vpk) {
        path = utf_to_wide(host.gui.game_selector.title_id);
    }

    if (path.empty()) {
        return Success;
    }

    Ptr<const void> entry_point;
    if (!load_app(entry_point, host, path, is_vpk)) {
        std::string message = "Failed to load \"";
        message += wide_to_utf(path);
        message += "\"";
        message += "\nSee console output for details.";
        error(message.c_str(), host.window.get());
        return ModuleLoadFailed;
    }

    const CallImport call_import = [&host](uint32_t nid, SceUID main_thread_id) {
        ::call_import(host, nid, main_thread_id);
    };

    const SceUID main_thread_id = create_thread(entry_point, host.kernel, host.mem, host.io.title_id.c_str(), SCE_KERNEL_DEFAULT_PRIORITY_USER, SCE_KERNEL_STACK_SIZE_USER_MAIN, call_import, false);
    if (main_thread_id < 0) {
        error("Failed to init main thread.", host.window.get());
        return InitThreadFailed;
    }

    const ThreadStatePtr main_thread = find(main_thread_id, host.kernel.threads);
    Ptr<void> argp = Ptr<void>();
    if (!strncmp(host.kernel.loaded_modules.begin()->second->module_name, "SceLibc", 7)) {
        const SceUID libc_thread_id = create_thread(host.kernel.loaded_modules.begin()->second->module_start, host.kernel, host.mem, "libc", SCE_KERNEL_DEFAULT_PRIORITY_USER, SCE_KERNEL_STACK_SIZE_USER_DEFAULT, call_import, false);
        const ThreadStatePtr libc_thread = find(libc_thread_id, host.kernel.threads);
        run_on_current(*libc_thread, host.kernel.loaded_modules.begin()->second->module_start, 0, argp);
        libc_thread->to_do = ThreadToDo::exit;
        libc_thread->something_to_do.notify_all(); // TODO Should this be notify_one()?
        host.kernel.running_threads.erase(libc_thread_id);
        host.kernel.threads.erase(libc_thread_id);
    }

    if (start_thread(host.kernel, main_thread_id, 0, Ptr<void>()) < 0) {
        error("Failed to run main thread.", host.window.get());
        return RunThreadFailed;
    }

    GLuint TextureID = 0;
    host.t1 = SDL_GetTicks();
    while (handle_events(host)) {
        if (!TextureID) {
            glGenTextures(1, &TextureID);
            glClearColor(1.0, 0.0, 0.5, 1.0);
            glClearDepth(1.0f);
            glViewport(0, 0, DEFAULT_RES_WIDTH, DEFAULT_RES_HEIGHT);
        }

        imgui::draw_begin(host.window);

        imgui::draw_main(host, fb_texture_id);

        DrawUI(host);
        DrawCommonDialog(host);

        imgui::draw_end(host.window);

        host.display.condvar.notify_all();

        const uint32_t t2 = SDL_GetTicks();
        const uint32_t ms = t2 - host.t1;
        if (ms >= 1000 && host.frame_count > 0) {
            const uint32_t fps = (host.frame_count * 1000) / ms;
            const uint32_t ms_per_frame = ms / host.frame_count;
            std::ostringstream title;
            title << window_title << " | " << host.game_title << " (" << host.io.title_id << ") | " << ms_per_frame << " ms/frame (" << fps << " frames/sec)";
            SDL_SetWindowTitle(host.window.get(), title.str().c_str());
            host.t1 = t2;
            host.frame_count = 0;
        }
    }
    return Success;
}
