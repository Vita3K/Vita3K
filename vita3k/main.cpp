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
#include <host/pkg.h>
#include <host/state.h>
#include <renderer/functions.h>
#include <shader/spirv_recompiler.h>
#include <util/log.h>
#include <util/string_utils.h>

#if DISCORD_RPC
#include <app/discord.h>
#endif

#ifdef WIN32
#include <combaseapi.h>
#endif

#include <SDL.h>
#include <cstdlib>

int main(int argc, char *argv[]) {
    Root root_paths;
    root_paths.set_base_path(SDL_GetBasePath());
    root_paths.set_pref_path(SDL_GetPrefPath(org_name, app_name));

    // Create default preference path for safety
    if (!fs::exists(root_paths.get_pref_path()))
        fs::create_directories(root_paths.get_pref_path());

    if (logging::init(root_paths, true) != Success)
        return InitConfigFailed;

    Config cfg{};
    if (const auto err = config::init_config(cfg, argc, argv, root_paths) != Success) {
        if (err == QuitRequested) {
            if (cfg.recompile_shader_path.is_initialized()) {
                LOG_INFO("Recompiling {}", *cfg.recompile_shader_path);
                shader::convert_gxp_to_glsl_from_filepath(*cfg.recompile_shader_path);
            }
            if (cfg.delete_title_id.is_initialized()) {
                LOG_INFO("Deleting title id {}", *cfg.delete_title_id);
                fs::remove_all(fs::path(root_paths.get_pref_path()) / "ux0/app" / *cfg.delete_title_id);
                fs::remove_all(fs::path(root_paths.get_pref_path()) / "ux0/addcont" / *cfg.delete_title_id);
                fs::remove_all(fs::path(root_paths.get_pref_path()) / "ux0/user/00/savedata" / *cfg.delete_title_id);
                fs::remove_all(fs::path(root_paths.get_pref_path()) / "shaderlog" / *cfg.delete_title_id);
            }
            HostState host;
            if (cfg.pkg_path.is_initialized() && cfg.pkg_zrif.is_initialized()) {
                LOG_INFO("Installing pkg from {} ", *cfg.pkg_path);
                install_pkg(*cfg.pkg_path, host, *cfg.pkg_zrif);
                return Success;
            }
            return Success;
        }
        return InitConfigFailed;
    }


#ifdef WIN32
    CoInitializeEx(NULL, COINIT_MULTITHREADED);
#endif

    if (cfg.console) {
        cfg.show_gui = false;
        if (logging::init(root_paths, false) != Success)
            return InitConfigFailed;
    } else {
        std::atexit(SDL_Quit);
        if (SDL_Init(SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER | SDL_INIT_HAPTIC | SDL_INIT_VIDEO) < 0) {
            app::error_dialog("SDL initialisation failed.");
            return SDLInitFailed;
        }
        SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    }

    LOG_INFO("{}", window_title);

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
    if (!app::init(host, cfg, root_paths)) {
        app::error_dialog("Host initialization failed.", host.window.get());
        return HostInitFailed;
    }

    GuiState gui;
    if (!cfg.console) {
        gui::init(gui, host);
#if DISCORD_RPC
        auto discord_rich_presence_old = host.cfg.discord_rich_presence;
#endif

        // Application not provided via argument, show game selector
        while (run_type == app::AppRunType::Unknown) {
            if (handle_events(host, gui)) {
                gui::draw_begin(gui, host);

#if DISCORD_RPC
                discord::update_init_status(host.cfg.discord_rich_presence, &discord_rich_presence_old);
#endif
                gui::draw_live_area(gui, host);
                gui::draw_ui(gui, host);

                gui::draw_end(gui, host.window.get());
            } else {
                return QuitRequested;
            }

            // TODO: Clean this, ie. make load_app overloads called depending on run type
            if (!gui.app_selector.selected_title_id.empty()) {
                vpk_path_wide = string_utils::utf_to_wide(gui.app_selector.selected_title_id);
                run_type = app::AppRunType::Extracted;
            }
        }
        gui::init_app_background(gui, host);
        host.renderer->features.hardware_flip = host.cfg.hardware_flip;
    }

    if (run_type == app::AppRunType::Vpk) {
        auto gui_ptr = cfg.console ? nullptr : &gui;
        if (!install_archive(host, gui_ptr, vpk_path_wide)) {
            return FileNotFound;
        }
    } else if (run_type == app::AppRunType::Extracted) {
        host.io.title_id = string_utils::wide_to_utf(vpk_path_wide);
    }

    Ptr<const void> entry_point;
    if (const auto err = load_app(entry_point, host, vpk_path_wide, run_type) != Success)
        return err;
    if (const auto err = run_app(host, entry_point) != Success)
        return err;

    if (cfg.console) {
        auto main_thread = host.kernel.threads.at(host.main_thread_id);
        auto lock = std::unique_lock<std::mutex>(main_thread->mutex);
        main_thread->something_to_do.wait(lock, [&]() {
            return main_thread->to_do == ThreadToDo::exit;
        });
        return Success;
    } else {
        gui.imgui_state->do_clear_screen = false;
    }

    app::gl_screen_renderer gl_renderer;

    if (!gl_renderer.init(host.base_path))
        return RendererInitFailed;

    while (host.frame_count == 0) {
        // Driver acto!
        renderer::process_batches(*host.renderer.get(), host.renderer->features, host.mem, host.cfg, host.base_path.c_str(),
            host.io.title_id.c_str());

        gl_renderer.render(host);

        gui::draw_begin(gui, host);
        gui::draw_common_dialog(gui, host);

        if (gui.apps_background.find(host.io.title_id) != gui.apps_background.end())
            // Display application background
            ImGui::GetForegroundDrawList()->AddImage(gui.apps_background[host.io.title_id],
                ImVec2(0.f, 0.f), ImGui::GetIO().DisplaySize);
        else if (!gui.user_backgrounds.empty())
            // Application background not found, Display user background if exist
            ImGui::GetForegroundDrawList()->AddImage(gui.user_backgrounds[host.cfg.user_backgrounds[gui.current_user_bg]],
                ImVec2(0.f, 0.f), ImGui::GetIO().DisplaySize);

        host.display.condvar.notify_all();
        gui::draw_end(gui, host.window.get());

        SDL_SetWindowTitle(host.window.get(), fmt::format("{} | {} ({}) | Please wait, loading...", window_title, host.app_title, host.io.title_id).c_str());
    }

    while (handle_events(host, gui)) {
        // Driver acto!
        renderer::process_batches(*host.renderer.get(), host.renderer->features, host.mem, host.cfg, host.base_path.c_str(),
            host.io.title_id.c_str());

        gl_renderer.render(host);

        // Calculate FPS
        app::calculate_fps(host);

        gui::draw_begin(gui, host);
        gui::draw_common_dialog(gui, host);
        gui::draw_live_area(gui, host);

        if (host.cfg.performance_overlay)
            gui::draw_perf_overlay(gui, host);

        gui::draw_trophies_unlocked(gui, host);
        if (host.display.imgui_render) {
            gui::draw_ui(gui, host);
        }

        host.display.condvar.notify_all();
        gui::draw_end(gui, host.window.get());
        app::set_window_title(host);
    }

#ifdef WIN32
    CoUninitialize();
#endif

    app::destroy(host, gui.imgui_state.get());

    return Success;
}
