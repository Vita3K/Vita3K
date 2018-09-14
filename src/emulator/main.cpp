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

#include <gui/functions.h>
#include <gui/imgui_impl.h>
#include <host/app.h>
#include <host/config.h>
#include <host/functions.h>
#include <host/screen_render.h>
#include <host/state.h>
#include <host/version.h>
#include <kernel/thread/thread_functions.h>
#include <util/log.h>
#include <util/string_utils.h>

#include <SDL.h>

#include <cstdlib>
#include <iterator>
#include <utility>

int main(int argc, char *argv[]) {
    logging::init();

    Config cfg{};
    if (const bool ret = config::init(cfg, argc, argv))
        return ret;

    LOG_INFO("{}", window_title);

    std::atexit(SDL_Quit);
    if (SDL_Init(SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER | SDL_INIT_VIDEO) < 0) {
        error_dialog("SDL initialisation failed.");
        return SDLInitFailed;
    }

    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

    AppRunType run_type;
    if (cfg.run_title_id)
        run_type = AppRunType::Extracted;
    else if (cfg.vpk_path)
        run_type = AppRunType::Vpk;
    else
        run_type = AppRunType::Unknown;

    std::wstring vpk_path_wide;
    if (run_type == AppRunType::Vpk) {
        vpk_path_wide = string_utils::utf_to_wide(*cfg.vpk_path);
    } else {
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_DROPFILE) {
                vpk_path_wide = string_utils::utf_to_wide(ev.drop.file);
                SDL_free(ev.drop.file);
                break;
            }
        }
    }

    // TODO: Clean this, ie. make load_app overloads called depending on run type
    if (run_type == AppRunType::Extracted) {
        vpk_path_wide = string_utils::utf_to_wide(*cfg.run_title_id);
    }

    HostState host;
    if (!init(host, std::move(cfg))) {
        error_dialog("Host initialisation failed.", host.window.get());
        return HostInitFailed;
    }

    imgui::init(host.window.get());

    // Application not provided via argument, show game selector
    while (run_type == AppRunType::Unknown) {
        if (handle_events(host)) {
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            imgui::draw_begin(host);

            DrawUI(host);
            DrawGameSelector(host, &run_type);

            imgui::draw_end(host.window.get());
        } else {
            return QuitRequested;
        }

        // TODO: Clean this, ie. make load_app overloads called depending on run type
        if (run_type == AppRunType::Extracted) {
            vpk_path_wide = string_utils::utf_to_wide(host.gui.game_selector.selected_title_id);
        }
    }

    Ptr<const void> entry_point;
    if (auto err = load_app(entry_point, host, vpk_path_wide, run_type) != Success)
        return err;

    if (auto err = run_app(host, entry_point) != Success)
        return err;

    gl_screen_renderer gl_renderer;

    if (!gl_renderer.init(host.base_path))
        return RendererInitFailed;

    while (handle_events(host)) {
        gl_renderer.render(host);
        imgui::draw_begin(host);
        DrawCommonDialog(host);
        if (host.display.imgui_render) {
            DrawUI(host);
        }
        imgui::draw_end(host.window.get());

        host.display.condvar.notify_all();

        set_window_title(host);
    }

    return Success;
}
