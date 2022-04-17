// Vita3K emulator project
// Copyright (C) 2022 Vita3K team
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
#include <config/functions.h>
#include <config/version.h>
#include <gui/functions.h>
#include <gui/state.h>
#include <host/functions.h>
#include <host/pkg.h>
#include <host/state.h>
#include <modules/module_parent.h>
#include <renderer/functions.h>
#include <renderer/gl/functions.h>
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

#include "Tracy.hpp"
#include <SDL.h>
#include <chrono>
#include <cstdlib>
#include <thread>

static void run_execv(char *argv[], HostState &host) {
    char *args[10];
    args[0] = argv[0];
    args[1] = (char *)"-a";
    args[2] = (char *)"true";
    if (!host.load_app_path.empty()) {
        args[3] = (char *)"-r";
        args[4] = host.load_app_path.data();
        if (!host.load_exec_path.empty()) {
            args[5] = (char *)"--self";
            args[6] = host.load_exec_path.data();
            if (!host.load_exec_argv.empty()) {
                args[7] = (char *)"--app-args";
                args[8] = host.load_exec_argv.data();
                args[9] = NULL;
            } else
                args[7] = NULL;
        } else
            args[5] = NULL;
    } else
        args[3] = NULL;

#ifdef WIN32
    _execv(argv[0], args);
#elif defined(__unix__) || defined(__APPLE__) && defined(__MACH__)
    execv(argv[0], args);
#endif
};

int main(int argc, char *argv[]) {
    ZoneScoped; // Tracy - Track main function scope
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
            if (cfg.recompile_shader_path.has_value()) {
                LOG_INFO("Recompiling {}", *cfg.recompile_shader_path);
                shader::convert_gxp_to_glsl_from_filepath(*cfg.recompile_shader_path);
            }
            if (cfg.delete_title_id.has_value()) {
                LOG_INFO("Deleting title id {}", *cfg.delete_title_id);
                fs::remove_all(fs::path(root_paths.get_pref_path()) / "ux0/app" / *cfg.delete_title_id);
                fs::remove_all(fs::path(root_paths.get_pref_path()) / "ux0/addcont" / *cfg.delete_title_id);
                fs::remove_all(fs::path(root_paths.get_pref_path()) / "ux0/user/00/savedata" / *cfg.delete_title_id);
                fs::remove_all(fs::path(root_paths.get_base_path()) / "cache/shaders" / *cfg.delete_title_id);
            }
            if (cfg.pkg_path.has_value() && cfg.pkg_zrif.has_value()) {
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

        // Enable HIDAPI rumble for DS4/DS
        SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_PS4_RUMBLE, "1");
        SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_PS5_RUMBLE, "1");

        // Enable Switch controller
        SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_SWITCH, "1");
        SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_JOY_CONS, "1");

        if (SDL_Init(SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER | SDL_INIT_VIDEO) < 0) {
            app::error_dialog("SDL initialisation failed.");
            return SDLInitFailed;
        }
        SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    }

    LOG_INFO("{}", window_title);
    LOG_INFO("Number of logical CPU cores: {}", SDL_GetCPUCount());
    LOG_INFO("Available ram memory: {} mo", SDL_GetSystemRAM());

    app::AppRunType run_type = app::AppRunType::Unknown;
    if (cfg.run_app_path)
        run_type = app::AppRunType::Extracted;

    if (!app::init(host, cfg, root_paths)) {
        app::error_dialog("Host initialization failed.", host.window.get());
        return HostInitFailed;
    }

    init_libraries(host);

    GuiState gui;
    if (!cfg.console) {
        gui::pre_init(gui, host);
        if (!host.cfg.initial_setup) {
            while (!host.cfg.initial_setup) {
                if (handle_events(host, gui)) {
                    gui::draw_begin(gui, host);
                    gui::draw_initial_setup(gui, host);
                    gui::draw_end(gui, host.window.get());
                } else
                    return QuitRequested;
            }
            config::serialize_config(host.cfg, host.base_path);
            run_execv(argv, host);
        }
        gui::init(gui, host);
    }

    if (cfg.content_path.has_value()) {
        auto gui_ptr = cfg.console ? nullptr : &gui;
        const auto extention = string_utils::tolower(cfg.content_path->extension().string());
        const auto is_archive = (extention == ".vpk") || (extention == ".zip");
        const auto is_rif = (extention == ".rif") || (extention == "work.bin");
        const auto is_directory = fs::is_directory(*cfg.content_path);

        const auto content_is_app = [&]() {
            std::vector<ContentInfo> contents_info = install_archive(host, gui_ptr, string_utils::utf_to_wide(cfg.content_path->string()));
            const auto content_index = std::find_if(contents_info.begin(), contents_info.end(), [&](const ContentInfo &c) {
                return c.category == "gd";
            });
            if ((content_index != contents_info.end()) && content_index->state) {
                host.app_title_id = content_index->title_id;
                return true;
            }

            return false;
        };
        if ((is_archive && content_is_app()) || (is_directory && (install_contents(host, gui_ptr, *cfg.content_path) == 1) && (host.app_category == "gd")))
            run_type = app::AppRunType::Extracted;
        else {
            if (is_rif)
                copy_license(host, *cfg.content_path);
            else if (!is_archive && !is_directory)
                LOG_ERROR("File dropped: [{}] is not supported.", cfg.content_path->string());

            host.cfg.content_path.reset();
            if (!cfg.console)
                gui::init_home(gui, host);
        }
    }

    if (run_type == app::AppRunType::Extracted) {
        host.io.app_path = cfg.run_app_path ? *cfg.run_app_path : host.app_title_id;
        gui::init_user_app(gui, host, host.io.app_path);
        if (host.cfg.run_app_path.has_value())
            host.cfg.run_app_path.reset();
        else if (host.cfg.content_path.has_value())
            host.cfg.content_path.reset();
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
                ZoneScopedN("UI rendering"); // Tracy - Track UI rendering loop scope
                gui::draw_begin(gui, host);

#if USE_DISCORD
                discordrpc::update_init_status(host.cfg.discord_rich_presence, &discord_rich_presence_old);
#endif
                gui::draw_live_area(gui, host);
                gui::draw_ui(gui, host);

                gui::draw_end(gui, host.window.get());
                FrameMark; // Tracy - Frame end mark for UI rendering loop
            } else {
                return QuitRequested;
            }

            if (!host.io.app_path.empty())
                run_type = app::AppRunType::Extracted;
        }
    }

    gui::set_config(gui, host, host.io.app_path);

    const auto APP_INDEX = gui::get_app_index(gui, host.io.app_path);
    host.app_version = APP_INDEX->app_ver;
    host.app_category = APP_INDEX->category;
    host.app_content_id = APP_INDEX->content_id;
    host.io.addcont = APP_INDEX->addcont;
    host.io.savedata = APP_INDEX->savedata;
    host.current_app_title = APP_INDEX->title;
    host.app_short_title = APP_INDEX->stitle;
    host.io.title_id = APP_INDEX->title_id;

    // Check license for PS App Only
    if (host.io.title_id.find("PCS") != std::string::npos)
        host.app_sku_flag = get_license_sku_flag(host, host.app_content_id);

    if (cfg.console) {
        auto main_thread = host.kernel.threads.at(host.main_thread_id);
        auto lock = std::unique_lock<std::mutex>(main_thread->mutex);
        main_thread->status_cond.wait(lock, [&]() {
            return main_thread->status == ThreadStatus::dormant;
        });
        return Success;
    } else {
        gui.imgui_state->do_clear_screen = false;
    }

    gui::init_app_background(gui, host, host.io.app_path);
    gui::update_last_time_app_used(gui, host, host.io.app_path);

    const auto draw_app_background = [&](GuiState &gui, HostState &host) {
        if (gui.apps_background.find(host.io.app_path) != gui.apps_background.end())
            // Display application background
            ImGui::GetBackgroundDrawList()->AddImage(gui.apps_background[host.io.app_path],
                ImVec2(0.f, 0.f), ImGui::GetIO().DisplaySize);
        // Application background not found
        else if (!gui.theme_backgrounds.empty())
            // Display theme background if exist
            ImGui::GetBackgroundDrawList()->AddImage(gui.theme_backgrounds[0], ImVec2(0.f, 0.f), ImGui::GetIO().DisplaySize);
        else if (!gui.user_backgrounds.empty())
            // Display user background if exist
            ImGui::GetBackgroundDrawList()->AddImage(gui.user_backgrounds[gui.users[host.io.user_id].backgrounds[0]],
                ImVec2(0.f, 0.f), ImGui::GetIO().DisplaySize);
    };

    Ptr<const void> entry_point;
    if (const auto err = load_app(entry_point, host, string_utils::utf_to_wide(host.io.app_path)) != Success)
        return err;

    // Pre-Compile Shader only for glsl, spriv is broken
    if (!host.cfg.spirv_shader) {
        auto &glstate = static_cast<renderer::gl::GLState &>(*host.renderer);
        if (renderer::gl::get_shaders_cache_hashs(glstate, host.base_path.c_str(), host.io.title_id.c_str(), host.self_name.c_str()) && cfg.shader_cache) {
            for (const auto &hash : glstate.shaders_cache_hashs) {
                gui::draw_begin(gui, host);
                draw_app_background(gui, host);

                renderer::gl::pre_compile_program(glstate, host.base_path.c_str(), host.io.title_id.c_str(), host.self_name.c_str(), hash);
                gui::draw_pre_compiling_shaders_progress(gui, host, uint32_t(glstate.shaders_cache_hashs.size()));

                gui::draw_end(gui, host.window.get());
                SDL_SetWindowTitle(host.window.get(), fmt::format("{} | {} ({}) | Please wait, compiling shaders...", window_title, host.current_app_title, host.io.title_id).c_str());
            }
        }
    }

    if (const auto err = run_app(host, entry_point) != Success)
        return err;

    while (host.frame_count == 0 && !host.load_exec) {
        // Driver acto!
        renderer::process_batches(*host.renderer.get(), host.renderer->features, host.mem, host.cfg, host.base_path.c_str(),
            host.io.title_id.c_str(), host.self_name.c_str());

        {
            const std::lock_guard<std::mutex> guard(host.display.display_info_mutex);
            host.renderer->render_frame(host.viewport_pos, host.viewport_size, host.display, host.gxm, host.mem);
        }

        gui::draw_begin(gui, host);
        gui::draw_common_dialog(gui, host);
        draw_app_background(gui, host);

        gui::draw_end(gui, host.window.get());

        SDL_SetWindowTitle(host.window.get(), fmt::format("{} | {} ({}) | Please wait, loading...", window_title, host.current_app_title, host.io.title_id).c_str());
    }

    while (handle_events(host, gui) && !host.load_exec) {
        ZoneScopedN("Game rendering"); // Tracy - Track game rendering loop scope
        // Driver acto!
        renderer::process_batches(*host.renderer.get(), host.renderer->features, host.mem, host.cfg, host.base_path.c_str(),
            host.io.title_id.c_str(), host.self_name.c_str());

        {
            const std::lock_guard<std::mutex> guard(host.display.display_info_mutex);
            host.renderer->render_frame(host.viewport_pos, host.viewport_size, host.display, host.gxm, host.mem);
        }

        // Calculate FPS
        app::calculate_fps(host);

        // Set shaders compiled display
        gui::set_shaders_compiled_display(gui, host);

        gui::draw_begin(gui, host);
        gui::draw_common_dialog(gui, host);
        gui::draw_live_area(gui, host);

        if (host.cfg.performance_overlay && !gui.live_area.app_selector && !gui.live_area.live_area_screen && gui::get_sys_apps_state(gui))
            gui::draw_perf_overlay(gui, host);

        if (host.display.imgui_render) {
            gui::draw_ui(gui, host);
        }

        gui::draw_end(gui, host.window.get());
        FrameMark; // Tracy - Frame end mark for game rendering loop
    }

#ifdef WIN32
    CoUninitialize();
#endif

    app::destroy(host, gui.imgui_state.get());

    if (host.load_exec)
        run_execv(argv, host);

    return Success;
}
