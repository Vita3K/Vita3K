// Vita3K emulator project
// Copyright (C) 2024 Vita3K team
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
#include <display/state.h>
#include <emuenv/state.h>
#include <gui/functions.h>
#include <gui/state.h>
#include <include/cpu.h>
#include <include/environment.h>
#include <io/state.h>
#include <kernel/state.h>
#include <modules/module_parent.h>
#include <packages/functions.h>
#include <packages/pkg.h>
#include <packages/sfo.h>
#include <renderer/functions.h>
#include <renderer/shaders.h>
#include <renderer/state.h>
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
#include <tracy/Tracy.hpp>

static void run_execv(char *argv[], EmuEnvState &emuenv) {
    char const *args[10];
    args[0] = argv[0];
    args[1] = "-a";
    args[2] = "true";
    if (!emuenv.load_app_path.empty()) {
        args[3] = "-r";
        args[4] = emuenv.load_app_path.data();
        if (!emuenv.load_exec_path.empty()) {
            args[5] = "--self";
            args[6] = emuenv.load_exec_path.data();
            if (!emuenv.load_exec_argv.empty()) {
                args[7] = "--app-args";
                args[8] = emuenv.load_exec_argv.data();
                args[9] = nullptr;
            } else
                args[7] = nullptr;
        } else
            args[5] = nullptr;
    } else
        args[3] = nullptr;

        // Execute the emulator again with some arguments
#ifdef WIN32
    FreeConsole();
    _execv(argv[0], args);
#elif defined(__unix__) || defined(__APPLE__) && defined(__MACH__)
    execv(argv[0], const_cast<char *const *>(args));
#endif
};

int main(int argc, char *argv[]) {
    ZoneScoped; // Tracy - Track main function scope
    Root root_paths;

    app::init_paths(root_paths);

    if (logging::init(root_paths, true) != Success)
        return InitConfigFailed;

    // Check admin privs before init starts to avoid creating of file as other user by accident
    bool adminPriv = false;
#ifdef WIN32
    // https://stackoverflow.com/questions/8046097/how-to-check-if-a-process-has-the-administrative-rights
    HANDLE hToken = NULL;
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken)) {
        TOKEN_ELEVATION Elevation;
        DWORD cbSize = sizeof(TOKEN_ELEVATION);
        if (GetTokenInformation(hToken, TokenElevation, &Elevation, sizeof(Elevation), &cbSize)) {
            adminPriv = Elevation.TokenIsElevated;
        }
    }
    if (hToken) {
        CloseHandle(hToken);
    }
#else
    auto uid = getuid();
    auto euid = geteuid();

    // if either effective uid or uid is the one of the root user assume running as root.
    // else if euid and uid are different then permissions errors can happen if its running
    // as a completely different user than the uid/euid
    if (uid == 0 || euid == 0 || uid != euid)
        adminPriv = true;
#endif

    if (adminPriv) {
        LOG_CRITICAL("PLEASE. DO NOT RUN VITA3K AS ADMIN OR WITH ADMIN PRIVILEGES.");
    }

    Config cfg{};
    EmuEnvState emuenv;
    const auto config_err = config::init_config(cfg, argc, argv, root_paths);

    fs::create_directories(cfg.get_pref_path());

    if (config_err != Success) {
        if (config_err == QuitRequested) {
            if (cfg.recompile_shader_path.has_value()) {
                LOG_INFO("Recompiling {}", *cfg.recompile_shader_path);
                shader::convert_gxp_to_glsl_from_filepath(*cfg.recompile_shader_path);
            }
            if (cfg.delete_title_id.has_value()) {
                LOG_INFO("Deleting title id {}", *cfg.delete_title_id);
                fs::remove_all(cfg.get_pref_path() / "ux0/app" / *cfg.delete_title_id);
                fs::remove_all(cfg.get_pref_path() / "ux0/addcont" / *cfg.delete_title_id);
                fs::remove_all(cfg.get_pref_path() / "ux0/user/00/savedata" / *cfg.delete_title_id);
                fs::remove_all(root_paths.get_cache_path() / "shaders" / *cfg.delete_title_id);
            }
            if (cfg.pup_path.has_value()) {
                LOG_INFO("Installing firmware file {}", *cfg.pup_path);
                install_pup(cfg.get_pref_path(), *cfg.pup_path, [](uint32_t progress) {
                    LOG_INFO("Firmware installation progress: {}%", progress);
                });
            }
            if (cfg.pkg_path.has_value() && cfg.pkg_zrif.has_value()) {
                LOG_INFO("Installing pkg from {} ", *cfg.pkg_path);
                emuenv.cache_path = root_paths.get_cache_path().generic_path();
                emuenv.pref_path = cfg.get_pref_path();
                auto pkg_path = fs_utils::utf8_to_path(*cfg.pkg_path);
                install_pkg(pkg_path, emuenv, *cfg.pkg_zrif, [](float) {});
            }
            return Success;
        }
        LOG_ERROR("Failed to initialise config");
        return InitConfigFailed;
    }

#ifdef WIN32
    {
        auto res = CoInitializeEx(NULL, COINIT_MULTITHREADED);
        LOG_ERROR_IF(res == S_FALSE, "Failed to initialize COM Library");
    }
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

        if (SDL_Init(SDL_INIT_GAMECONTROLLER | SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0) {
            app::error_dialog("SDL initialisation failed.");
            return SDLInitFailed;
        }
        SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    }

    LOG_INFO("{}", window_title);
    LOG_INFO("OS: {}", CppCommon::Environment::OSVersion());
    LOG_INFO("CPU: {} | {} Threads | {} GHz", CppCommon::CPU::Architecture(), CppCommon::CPU::LogicalCores(), static_cast<float>(CppCommon::CPU::ClockSpeed()) / 1000.f);
    LOG_INFO("Available ram memory: {} MiB", SDL_GetSystemRAM());

    app::AppRunType run_type = app::AppRunType::Unknown;
    if (cfg.run_app_path)
        run_type = app::AppRunType::Extracted;

    if (!app::init(emuenv, cfg, root_paths)) {
        app::error_dialog("Emulated environment initialization failed.", emuenv.window.get());
        return 1;
    }

    if (emuenv.cfg.controller_binds.empty() || (emuenv.cfg.controller_binds.size() != 15))
        gui::reset_controller_binding(emuenv);

    init_libraries(emuenv);

    GuiState gui;
    if (!cfg.console) {
        gui::pre_init(gui, emuenv);
        if (!emuenv.cfg.initial_setup) {
            while (!emuenv.cfg.initial_setup) {
                if (handle_events(emuenv, gui)) {
                    gui::draw_begin(gui, emuenv);
                    gui::draw_initial_setup(gui, emuenv);
                    gui::draw_end(gui);
                    emuenv.renderer->swap_window(emuenv.window.get());
                } else
                    return QuitRequested;
            }
            config::serialize_config(emuenv.cfg, emuenv.config_path);
            run_execv(argv, emuenv);
        }
        gui::init(gui, emuenv);
    }

    if (cfg.content_path.has_value()) {
        auto gui_ptr = cfg.console ? nullptr : &gui;
        const auto extention = string_utils::tolower(cfg.content_path->extension().string());
        const auto is_archive = (extention == ".vpk") || (extention == ".zip");
        const auto is_rif = (extention == ".rif") || (extention == "work.bin");
        const auto is_directory = fs::is_directory(*cfg.content_path);

        const auto content_is_app = [&]() {
            std::vector<ContentInfo> contents_info = install_archive(emuenv, gui_ptr, *cfg.content_path);
            const auto content_index = std::find_if(contents_info.begin(), contents_info.end(), [&](const ContentInfo &c) {
                return c.category == "gd";
            });
            if ((content_index != contents_info.end()) && content_index->state) {
                emuenv.app_info.app_title_id = content_index->title_id;
                return true;
            }

            return false;
        };
        if ((is_archive && content_is_app()) || (is_directory && (install_contents(emuenv, gui_ptr, *cfg.content_path) == 1) && (emuenv.app_info.app_category == "gd")))
            run_type = app::AppRunType::Extracted;
        else {
            if (is_rif)
                copy_license(emuenv, *cfg.content_path);
            else if (!is_archive && !is_directory)
                LOG_ERROR("File dropped: [{}] is not supported.", *cfg.content_path);

            emuenv.cfg.content_path.reset();
            if (!cfg.console)
                gui::init_home(gui, emuenv);
        }
    }

    if (run_type == app::AppRunType::Extracted) {
        emuenv.io.app_path = cfg.run_app_path ? *cfg.run_app_path : emuenv.app_info.app_title_id;
        gui::init_user_app(gui, emuenv, emuenv.io.app_path);
        if (emuenv.cfg.run_app_path.has_value())
            emuenv.cfg.run_app_path.reset();
        else if (emuenv.cfg.content_path.has_value())
            emuenv.cfg.content_path.reset();
    }

    if (!cfg.console) {
#if USE_DISCORD
        auto discord_rich_presence_old = emuenv.cfg.discord_rich_presence;
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

            if (handle_events(emuenv, gui)) {
                ZoneScopedN("UI rendering"); // Tracy - Track UI rendering loop scope
                gui::draw_begin(gui, emuenv);

#if USE_DISCORD
                discordrpc::update_init_status(emuenv.cfg.discord_rich_presence, &discord_rich_presence_old);
#endif
                gui::draw_vita_area(gui, emuenv);
                gui::draw_ui(gui, emuenv);

                gui::draw_end(gui);
                emuenv.renderer->swap_window(emuenv.window.get());
                FrameMark; // Tracy - Frame end mark for UI rendering loop
            } else {
                return QuitRequested;
            }

            if (!emuenv.io.app_path.empty()) {
                run_type = app::AppRunType::Extracted;
                gui.vita_area.home_screen = false;
                gui.vita_area.live_area_screen = false;
            }
        }
    }

    // When backend render is changed before boot app, reboot emu in new backend render and run app
    if (emuenv.renderer->current_backend != emuenv.backend_renderer) {
        emuenv.load_app_path = emuenv.io.app_path;
        run_execv(argv, emuenv);
        return Success;
    }

    gui::set_config(gui, emuenv, emuenv.io.app_path);

    const auto APP_INDEX = gui::get_app_index(gui, emuenv.io.app_path);
    emuenv.app_info.app_version = APP_INDEX->app_ver;
    emuenv.app_info.app_category = APP_INDEX->category;
    emuenv.app_info.app_content_id = APP_INDEX->content_id;
    emuenv.io.addcont = APP_INDEX->addcont;
    emuenv.io.savedata = APP_INDEX->savedata;
    emuenv.current_app_title = APP_INDEX->title;
    emuenv.app_info.app_short_title = APP_INDEX->stitle;
    emuenv.io.title_id = APP_INDEX->title_id;

    // Check license for PS App Only
    if (emuenv.io.title_id.starts_with("PCS"))
        emuenv.app_sku_flag = get_license_sku_flag(emuenv, emuenv.app_info.app_content_id);

    if (cfg.console) {
        auto main_thread = emuenv.kernel.get_thread(emuenv.main_thread_id);
        auto lock = std::unique_lock<std::mutex>(main_thread->mutex);
        main_thread->status_cond.wait(lock, [&]() {
            return main_thread->status == ThreadStatus::dormant;
        });
        return Success;
    } else {
        gui.imgui_state->do_clear_screen = false;
    }

    gui::init_app_background(gui, emuenv, emuenv.io.app_path);
    gui::update_last_time_app_used(gui, emuenv, emuenv.io.app_path);

    if (!app::late_init(emuenv)) {
        app::error_dialog("Failed to initialize Vita3K", emuenv.window.get());
        return 1;
    }

    const auto draw_app_background = [](GuiState &gui, EmuEnvState &emuenv) {
        const auto pos_min = ImVec2(emuenv.viewport_pos.x, emuenv.viewport_pos.y);
        const auto pos_max = ImVec2(pos_min.x + emuenv.viewport_size.x, pos_min.y + emuenv.viewport_size.y);

        if (gui.apps_background.contains(emuenv.io.app_path))
            // Display application background
            ImGui::GetBackgroundDrawList()->AddImage(gui.apps_background[emuenv.io.app_path], pos_min, pos_max);
        // Application background not found
        else
            gui::draw_background(gui, emuenv);
    };

    int32_t main_module_id;
    {
        const auto err = load_app(main_module_id, emuenv);
        if (err != Success)
            return err;
    }
    gui.vita_area.information_bar = false;

    // Pre-Compile Shaders
    emuenv.renderer->set_app(emuenv.io.title_id.c_str(), emuenv.self_name.c_str());
    if (renderer::get_shaders_cache_hashs(*emuenv.renderer) && cfg.shader_cache) {
        SDL_SetWindowTitle(emuenv.window.get(), fmt::format("{} | {} ({}) | Please wait, compiling shaders...", window_title, emuenv.current_app_title, emuenv.io.title_id).c_str());
        for (const auto &hash : emuenv.renderer->shaders_cache_hashs) {
            handle_events(emuenv, gui);
            gui::draw_begin(gui, emuenv);
            draw_app_background(gui, emuenv);

            emuenv.renderer->precompile_shader(hash);
            gui::draw_pre_compiling_shaders_progress(gui, emuenv, uint32_t(emuenv.renderer->shaders_cache_hashs.size()));

            gui::draw_end(gui);
            emuenv.renderer->swap_window(emuenv.window.get());
        }
    }
    {
        const auto err = run_app(emuenv, main_module_id);
        if (err != Success)
            return err;
    }
    SDL_SetWindowTitle(emuenv.window.get(), fmt::format("{} | {} ({}) | Please wait, loading...", window_title, emuenv.current_app_title, emuenv.io.title_id).c_str());

    while (handle_events(emuenv, gui) && (emuenv.frame_count == 0) && !emuenv.load_exec) {
        ZoneScopedN("Game loading"); // Tracy - Track game loading loop scope
        // Driver acto!
        renderer::process_batches(*emuenv.renderer.get(), emuenv.renderer->features, emuenv.mem, emuenv.cfg);

        const SceFVector2 viewport_pos = { emuenv.viewport_pos.x, emuenv.viewport_pos.y };
        const SceFVector2 viewport_size = { emuenv.viewport_size.x, emuenv.viewport_size.y };
        emuenv.renderer->render_frame(viewport_pos, viewport_size, emuenv.display, emuenv.gxm, emuenv.mem);

        gui::draw_begin(gui, emuenv);
        gui::draw_common_dialog(gui, emuenv);
        draw_app_background(gui, emuenv);

        gui::draw_end(gui);
        emuenv.renderer->swap_window(emuenv.window.get());
        FrameMark; // Tracy - Frame end mark for game loading loop
    }

    while (handle_events(emuenv, gui) && !emuenv.load_exec) {
        ZoneScopedN("Game rendering"); // Tracy - Track game rendering loop scope
        // Driver acto!
        renderer::process_batches(*emuenv.renderer.get(), emuenv.renderer->features, emuenv.mem, emuenv.cfg);

        const SceFVector2 viewport_pos = { emuenv.viewport_pos.x, emuenv.viewport_pos.y };
        const SceFVector2 viewport_size = { emuenv.viewport_size.x, emuenv.viewport_size.y };
        emuenv.renderer->render_frame(viewport_pos, viewport_size, emuenv.display, emuenv.gxm, emuenv.mem);
        // Calculate FPS
        app::calculate_fps(emuenv);

        // Set shaders compiled display
        gui::set_shaders_compiled_display(gui, emuenv);

        gui::draw_begin(gui, emuenv);
        if (!emuenv.kernel.is_threads_paused())
            gui::draw_common_dialog(gui, emuenv);
        gui::draw_vita_area(gui, emuenv);

        if (emuenv.cfg.performance_overlay && !emuenv.kernel.is_threads_paused() && (emuenv.common_dialog.status != SCE_COMMON_DIALOG_STATUS_RUNNING))
            gui::draw_perf_overlay(gui, emuenv);

        if (emuenv.cfg.current_config.show_touchpad_cursor && !emuenv.kernel.is_threads_paused())
            gui::draw_touchpad_cursor(emuenv);

        if (emuenv.display.imgui_render) {
            gui::draw_ui(gui, emuenv);
        }

        gui::draw_end(gui);
        emuenv.renderer->swap_window(emuenv.window.get());
        FrameMark; // Tracy - Frame end mark for game rendering loop
    }

#ifdef WIN32
    CoUninitialize();
#endif

    emuenv.renderer->preclose_action();
    app::destroy(emuenv, gui.imgui_state.get());

    if (emuenv.load_exec)
        run_execv(argv, emuenv);

    return Success;
}
