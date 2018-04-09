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

#include "vpk.h"

#include <host/functions.h>
#include <host/state.h>
#include <host/version.h>
#include <kernel/thread_functions.h>
#include <util/find.h>
#include <util/log.h>
#include <util/string_convert.h>

#include <SDL.h>
#include <glutil/gl.h>

#include <algorithm> // find_if_not
#include <cassert>
#include <iostream>
#include <sstream>

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

    if (path.empty()) {
        std::string message = "Usage: ";
        message += argv[0];
        message += " <path to VPK file>";
        error(message);
        return IncorrectArgs;
    }

    HostState host;
    if (!init(host)) {
        error("Host initialisation failed.", host.window.get());
        return HostInitFailed;
    }

    Ptr<const void> entry_point;
    if (!load_vpk(entry_point, host.game_title, host.title_id, host.kernel, host.io, host.mem, path)) {
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

    const size_t stack_size = MB(1); // TODO Get main thread stack size from somewhere?

    const SceUID main_thread_id = create_thread(entry_point, host.kernel, host.mem, "main", stack_size, call_import, false);
    if (main_thread_id < 0) {
        error("Failed to init main thread.", host.window.get());
        return InitThreadFailed;
    }

    const SceUID display_thread_id = create_thread(entry_point, host.kernel, host.mem, "display", stack_size, call_import, false);

    if (display_thread_id < 0) {
        error("Failed to init display thread.", host.window.get());
        return InitThreadFailed;
    }

    const ThreadStatePtr main_thread = find(main_thread_id, host.kernel.threads);
    Ptr<void> argp = Ptr<void>();
    if(!strncmp(host.kernel.loaded_modules.begin()->second->module_name,"SceLibc",7)){
        run_on_current(*main_thread, host.kernel.loaded_modules.begin()->second->module_start, 0, argp);
    }

    if (start_thread(host.kernel, main_thread_id, 0, Ptr<void>()) < 0) {
        error("Failed to run main thread.", host.window.get());
        return RunThreadFailed;
    }

    const ThreadStatePtr display_thread = find(display_thread_id, host.kernel.threads);

    GLuint TextureID = 0;
    host.t1 = SDL_GetTicks();
    while (handle_events(host)) {
        if (!TextureID) {
            glGenTextures(1, &TextureID);
            glClearColor(1.0, 0.0, 0.5, 1.0);
            glClearDepth(1.0f);
            glViewport(0, 0, 960, 544);
            glMatrixMode(GL_PROJECTION);
            glLoadIdentity();
            glOrtho(0, 960, 544, 0, 1, -1);
            glMatrixMode(GL_MODELVIEW);
            glEnable(GL_TEXTURE_2D);
            glLoadIdentity();
        }

        // Clear back buffer

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glLoadIdentity();

        {
            if (host.display.width > 0) {
                glBindTexture(GL_TEXTURE_2D, TextureID);
                void *const pixels = host.display.base.cast<void>().get(host.mem);

                glPixelStorei(GL_UNPACK_ROW_LENGTH, host.display.pitch);
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, host.display.width, host.display.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
                glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);

                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                glBindTexture(GL_TEXTURE_2D, TextureID);

                // For Ortho mode, of course
                const int X = 0;
                const int Y = 0;
                const int Width = 960;
                const int Height = 544;

                glBegin(GL_TRIANGLE_FAN);
                glTexCoord2f(0, 0);
                glVertex3f(X, Y, 0);
                glTexCoord2f(1, 0);
                glVertex3f(X + Width, Y, 0);
                glTexCoord2f(1, 1);
                glVertex3f(X + Width, Y + Height, 0);
                glTexCoord2f(0, 1);
                glVertex3f(X, Y + Height, 0);
                glEnd();
            }
        }

        SDL_GL_SwapWindow(host.window.get());

        {
            host.display.condvar.notify_all();
        }

        const uint32_t t2 = SDL_GetTicks();
        const uint32_t ms = t2 - host.t1;
        if (ms >= 1000 && host.frame_count > 0) {
            const uint32_t fps = (host.frame_count * 1000) / ms;
            const uint32_t ms_per_frame = ms / host.frame_count;
            std::ostringstream title;
            title << window_title << " | " << host.game_title << " (" << host.title_id << ") | " << ms_per_frame << " ms/frame (" << fps << " frames/sec)";
            SDL_SetWindowTitle(host.window.get(), title.str().c_str());
            host.t1 = t2;
            host.frame_count = 0;
        }
    }
    return Success;
}
