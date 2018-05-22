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
#include <util/log.h>
#include <util/string_convert.h>

#include <SDL.h>
#include <glutil/gl.h>

#include <algorithm> // find_if_not
#include <cassert>

#include <gui/functions.h>
#include <gui/imgui_impl.h>

int main(int argc, char *argv[]) {
    init_logging();

    LOG_INFO("{}", window_title);

    ProgramArgsWide argv_wide = process_args(argc, argv);

    const SDLPtr sdl(reinterpret_cast<const void *>(SDL_Init(SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER | SDL_INIT_VIDEO) >= 0), [](const void *succeeded) {
        assert(succeeded != nullptr);
        SDL_Quit();
    });
    if (!sdl) {
        error_dialog("SDL initialisation failed.");
        return SDLInitFailed;
    }

    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

    // Filter out an argument that macOS Finder appends 
    const char *const *const path_arg = std::find_if_not(&argv[1], &argv[argc], [](const char *arg) {
        return strncmp(arg, "-psn_", 5) == 0;
    });

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
    if (auto err = load_app(entry_point, host, path, is_vpk) != Success)
        return err;

    if (auto err = run_app(host, entry_point) != Success)
        return err;

    GLuint fb_texture_id = 0;
    host.sdl_ticks = SDL_GetTicks();
    while (handle_events(host)) {
        if (!fb_texture_id) {
            no_fb_fallback(host, &fb_texture_id);
        }

        imgui::draw_begin(host.window);

        imgui::draw_main(host, fb_texture_id);

        DrawUI(host);
        DrawCommonDialog(host);

        imgui::draw_end(host.window);

        host.display.condvar.notify_all();

        set_window_title(host);
    }
    return Success;
}
