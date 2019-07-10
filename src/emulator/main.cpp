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

#include "interface.h"

#include <app/functions.h>
#include <app/screen_render.h>
#include <config/functions.h>
#include <config/version.h>
#include <gui/functions.h>
#include <gui/state.h>
#include <renderer/functions.h>
#include <shader/spirv_recompiler.h>
#include <util/log.h>
#include <util/string_utils.h>

#include <SDL.h>

#ifdef USE_DISCORD_RICH_PRESENCE
#include <app/discord.h>
#endif

#ifdef WIN32
#include <combaseapi.h>
#endif

#include <cstdlib>
#include <SDL.h>

int main(int argc, char *argv[]) {
    Root root_paths;
    root_paths.set_base_path(SDL_GetBasePath());
    root_paths.set_pref_path(SDL_GetPrefPath(org_name, app_name));

    // Create default preference path for safety
    if (!fs::exists(root_paths.get_pref_path()))
        fs::create_directories(root_paths.get_pref_path());

    if (logging::init(root_paths) != Success)
        return InitConfigFailed;

    Config cfg{};
    if (const auto err = config::init_config(cfg, argc, argv, root_paths) != Success) {
        if (err == QuitRequested) {
            if (cfg.recompile_shader_path.is_initialized()) {
                LOG_INFO("Recompiling {}", *cfg.recompile_shader_path);
                shader::convert_gxp_to_glsl_from_filepath(*cfg.recompile_shader_path);
            }
            return Success;
        }
        return InitConfigFailed;
    }

    LOG_INFO("{}", window_title);

#ifdef WIN32
    CoInitializeEx(NULL, COINIT_MULTITHREADED);
#endif

    std::atexit(SDL_Quit);
    if (SDL_Init(SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER | SDL_INIT_HAPTIC | SDL_INIT_VIDEO) < 0) {
        app::error_dialog("SDL initialisation failed.");
        return SDLInitFailed;
    }

    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

    app::AppRunType run_type;
    if (cfg.run_title_id)
        run_type = app::AppRunType::Extracted;
    else if (cfg.vpk_path)
        run_type = app::AppRunType::Vpk;
    else
        run_type = app::AppRunType::Unknown;

    std::wstring vpk_path_wide;
    if (run_type == app::AppRunType::Vpk) {
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
    if (run_type == app::AppRunType::Extracted) {
        vpk_path_wide = string_utils::utf_to_wide(*cfg.run_title_id);
    }

    HostState host;
    if (!app::init(host, std::move(cfg), root_paths)) {
        app::error_dialog("Host initialisation failed.", host.window.get());
        return HostInitFailed;
    }

    GuiState gui;
    gui::init(gui, host);

    auto discord_rich_presence_old = host.cfg.discord_rich_presence;

    // Application not provided via argument, show game selector
    while (run_type == app::AppRunType::Unknown) {
        if (handle_events(host)) {
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            gui::draw_begin(gui, host);

#ifdef USE_DISCORD_RICH_PRESENCE
            discord::update_init_status(host.cfg.discord_rich_presence, &discord_rich_presence_old);
#endif
            gui::draw_ui(gui, host);
            gui::draw_game_selector(gui, host);

            gui::draw_end(host.window.get());
        } else {
            return QuitRequested;
        }

        // TODO: Clean this, ie. make load_app overloads called depending on run type
        if (!gui.game_selector.selected_title_id.empty()) {
            vpk_path_wide = string_utils::utf_to_wide(gui.game_selector.selected_title_id);
            run_type = app::AppRunType::Extracted;
        }
    }

    Ptr<const void> entry_point;
    if (const auto err = load_app(entry_point, host, gui, vpk_path_wide, run_type) != Success)
        return err;
    if (const auto err = run_app(host, entry_point) != Success)
        return err;

    app::gl_screen_renderer gl_renderer;

    if (!gl_renderer.init(host.base_path))
        return RendererInitFailed;

    while (handle_events(host)) {
        gl_renderer.render(host);

        // Driver acto!
        renderer::process_batches(*host.renderer.get(), host.features, host.mem, host.cfg, host.base_path.c_str(),
            host.io.title_id.c_str());

        gui::draw_begin(gui, host);
        gui::draw_common_dialog(gui, host);
        gui::draw_trophies_unlocked(gui);
        if (host.display.imgui_render) {
            gui::draw_ui(gui, host);
        }
        gui::draw_end(host.window.get());

        host.display.condvar.notify_all();

        app::set_window_title(host);
    }

#ifdef WIN32
    CoUninitialize();
#endif

    app::destory(host);

    return Success;
}
