// Vita3K emulator project
// Copyright (C) 2021 Vita3K team
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
#include <modules/module_parent.h>
#include <renderer/functions.h>
#include <shader/spirv_recompiler.h>
#include <util/log.h>
#include <util/string_utils.h>

#if USE_DISCORD
#include <app/discord.h>
#endif

#ifdef WIN32
#include <combaseapi.h>
#include <process.h>
#endif

#include <SDL.h>
#include <chrono>
#include <cstdlib>
#include <thread>

int main(int argc, char *argv[]) {
    Root root_paths;
    root_paths.set_base_path(string_utils::utf_to_wide(SDL_GetBasePath()));
    root_paths.set_pref_path(SDL_GetPrefPath(org_name, app_name));

    // Create default preference path for safety
    if (!fs::exists(root_paths.get_pref_path()))
        fs::create_directories(root_paths.get_pref_path());

    if (logging::init(root_paths, true) != Success)
        return InitConfigFailed;

    Config cfg{};
    HostState host;
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
            if (cfg.pkg_path.is_initialized() && cfg.pkg_zrif.is_initialized()) {
                LOG_INFO("Installing pkg from {} ", *cfg.pkg_path);
                host.pref_path = string_utils::utf_to_wide(root_paths.get_pref_path_string());
                install_pkg(*cfg.pkg_path, host, *cfg.pkg_zrif, [](float) {});
                return Success;
            }
            return Success;
        }
        return InitConfigFailed;
    }

#ifdef WIN32
    auto res = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    LOG_ERROR_IF(res == S_FALSE, "Failed to initialize COM Library");
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
    if (cfg.run_app_path)
        run_type = app::AppRunType::Extracted;
    else if (cfg.vpk_path)
        run_type = app::AppRunType::Vpk;
    else
        run_type = app::AppRunType::Unknown;

    SDL_Event ev;
    while (SDL_PollEvent(&ev)) {
        if (ev.type == SDL_DROPFILE) {
            *cfg.vpk_path = ev.drop.file;
            SDL_free(ev.drop.file);
            break;
        }
    }

    auto inject = create_cpu_dep_inject(host);
    if (!app::init(host, cfg, root_paths, inject)) {
        app::error_dialog("Host initialization failed.", host.window.get());
        return HostInitFailed;
    }

    GuiState gui;
    if (!cfg.console)
        gui::init(gui, host);

    if (run_type == app::AppRunType::Vpk) {
        auto gui_ptr = cfg.console ? nullptr : &gui;
        if (install_archive(host, gui_ptr, string_utils::utf_to_wide(*cfg.vpk_path)) && (host.app_category == "gd"))
            run_type = app::AppRunType::Extracted;
        else {
            run_type = app::AppRunType::Unknown;
            host.cfg.vpk_path.reset();
            if (!cfg.console)
                gui::init_home(gui, host);
        }
    }

    if (run_type == app::AppRunType::Extracted) {
        host.io.app_path = cfg.run_app_path ? *cfg.run_app_path : host.app_title_id;
        gui::get_user_app_params(gui, host, host.io.app_path);
        gui::init_apps_icon(gui, host, gui.app_selector.user_apps);
        if (host.cfg.run_app_path)
            host.cfg.run_app_path.reset();
    }

    if (!cfg.console) {
#if USE_DISCORD
        auto discord_rich_presence_old = host.cfg.discord_rich_presence;
#endif

        std::chrono::system_clock::time_point present = std::chrono::system_clock::now();
        std::chrono::system_clock::time_point later = std::chrono::system_clock::now();
        const double frame_time = 1000.0 / 60.0;
        // Application not provided via argument, show app selector
        while (run_type == app::AppRunType::Unknown) {
            // get the current time & get the time we worked for
            present = std::chrono::system_clock::now();
            std::chrono::duration<double, std::milli> work_time = present - later;
            // check if we are running faster than ~60fps (16.67ms)
            if (work_time.count() < frame_time) {
                // sleep for delta time.
                std::chrono::duration<double, std::milli> delta_ms(frame_time - work_time.count());
                auto delta_ms_duration = std::chrono::duration_cast<std::chrono::milliseconds>(delta_ms);
                std::this_thread::sleep_for(std::chrono::milliseconds(delta_ms_duration.count()));
            }
            // save the later time
            later = std::chrono::system_clock::now();

            if (handle_events(host, gui)) {
                gui::draw_begin(gui, host);

#if USE_DISCORD
                discordrpc::update_init_status(host.cfg.discord_rich_presence, &discord_rich_presence_old);
#endif
                gui::draw_live_area(gui, host);
                gui::draw_ui(gui, host);

                gui::draw_end(gui, host.window.get());
            } else {
                return QuitRequested;
            }

            if (!host.io.app_path.empty())
                run_type = app::AppRunType::Extracted;
        }
    }

    const auto APP_INDEX = gui::get_app_index(gui, host.io.app_path);
    host.app_version = APP_INDEX->app_ver;
    host.app_category = APP_INDEX->category;
    host.current_app_title = APP_INDEX->title;
    host.app_short_title = APP_INDEX->stitle;
    host.io.title_id = APP_INDEX->title_id;

    Ptr<const void> entry_point;
    if (const auto err = load_app(entry_point, host, string_utils::utf_to_wide(host.io.app_path)) != Success)
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

    gui::init_app_background(gui, host, host.io.app_path);

    app::gl_screen_renderer gl_renderer;

    if (!gl_renderer.init(host.base_path))
        return RendererInitFailed;

    while (host.frame_count == 0 && !host.load_exec) {
        // Driver acto!
        renderer::process_batches(*host.renderer.get(), host.renderer->features, host.mem, host.cfg, host.base_path.c_str(),
            host.io.title_id.c_str());

        gl_renderer.render(host);

        gui::draw_begin(gui, host);
        gui::draw_common_dialog(gui, host);

        if (gui.apps_background.find(host.io.app_path) != gui.apps_background.end())
            // Display application background
            ImGui::GetForegroundDrawList()->AddImage(gui.apps_background[host.io.app_path],
                ImVec2(0.f, 0.f), ImGui::GetIO().DisplaySize);
        // Application background not found
        else if (!gui.theme_backgrounds.empty())
            // Display theme background if exist
            ImGui::GetForegroundDrawList()->AddImage(gui.theme_backgrounds[0], ImVec2(0.f, 0.f), ImGui::GetIO().DisplaySize);
        else if (!gui.user_backgrounds.empty())
            // Display user background if exist
            ImGui::GetForegroundDrawList()->AddImage(gui.user_backgrounds[gui.users[host.io.user_id].backgrounds[0]],
                ImVec2(0.f, 0.f), ImGui::GetIO().DisplaySize);

        host.display.condvar.notify_all();
        gui::draw_end(gui, host.window.get());

        SDL_SetWindowTitle(host.window.get(), fmt::format("{} | {} ({}) | Please wait, loading...", window_title, host.current_app_title, host.io.title_id).c_str());
    }

    while (handle_events(host, gui) && !host.load_exec) {
        // Driver acto!
        renderer::process_batches(*host.renderer.get(), host.renderer->features, host.mem, host.cfg, host.base_path.c_str(),
            host.io.title_id.c_str());

        gl_renderer.render(host);

        // Calculate FPS
        app::calculate_fps(host);

        gui::draw_begin(gui, host);
        gui::draw_common_dialog(gui, host);
        gui::draw_live_area(gui, host);

        if (host.cfg.performance_overlay && !gui.live_area.app_selector && !gui.live_area.live_area_screen && gui::get_live_area_sys_app_state(gui))
            gui::draw_perf_overlay(gui, host);

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

    if (host.load_exec) {
        char *args[8];
        args[0] = argv[0];
        args[1] = (char *)"-r";
        args[2] = host.io.app_path.data();
        args[3] = (char *)"--self";
        args[4] = host.load_self_path.data();
        if (!host.load_exec_argv.empty()) {
            args[5] = (char *)"--console-arguments";
            args[6] = host.load_exec_argv.data();
            args[7] = NULL;
        } else
            args[5] = NULL;

#ifdef WIN32
        _execv(argv[0], args);
#elif defined(__unix__) || defined(__APPLE__) && defined(__MACH__)
        execv(argv[0], args);
#endif
    }

    return Success;
}
