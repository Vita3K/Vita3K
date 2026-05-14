<<<<<<< Updated upstream
// Vita3K emulator project
// Copyright (C) 2026 Vita3K team
=======
// RPCSV emulator project
// Copyright (C) 2025 RPCSV team
>>>>>>> Stashed changes
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

#include "archive.h"
#include "interface.h"

#include <app/functions.h>
#include <config/functions.h>
#include <config/version.h>
#include <emuenv/state.h>
#include <gui-qt/gui_language.h>
#include <gui-qt/gui_settings.h>
#include <gui-qt/log_widget.h>
#include <gui-qt/main_window.h>
#include <gui-qt/persistent_settings.h>
#include <include/cpu.h>
#include <include/environment.h>
#include <io/state.h>
#include <modules/module_parent.h>
#include <packages/functions.h>
#include <packages/license.h>
#include <packages/pkg.h>
#include <packages/sfo.h>
#include <shader/spirv_recompiler.h>
#include <util/log.h>
#include <util/string_utils.h>

#include <QApplication>
#include <QMessageBox>

#if USE_DISCORD
#include <app/discord.h>
#endif

#ifdef _WIN32
#include <combaseapi.h>
#define SDL_MAIN_HANDLED
#endif

#ifdef TRACY_ENABLE
#include <tracy/Tracy.hpp>
#endif

#include <SDL3/SDL_cpuinfo.h>
#include <SDL3/SDL_gamepad.h>
#include <SDL3/SDL_hints.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_main.h>

#include <cstdlib>
#include <optional>

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("Vita3K"));
    QCoreApplication::setApplicationName(QStringLiteral("Vita3K"));

#ifdef TRACY_ENABLE
    ZoneScoped; // Tracy - Track main function scope
#endif

    Root root_paths;
    app::init_paths(root_paths);

    if (!fs::exists(root_paths.get_pref_path())) {
        fs::create_directories(root_paths.get_pref_path());
    }

    LogWidget::register_callback();
    if (logging::init(root_paths, true) != Success) {
        return InitConfigFailed;
    }

    // Check admin privs before init starts to avoid creating of file as other user by accident
    bool admin_priv = false;
#ifdef _WIN32
    // https://stackoverflow.com/questions/8046097/how-to-check-if-a-process-has-the-administrative-rights
    HANDLE hToken = NULL;
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken)) {
        TOKEN_ELEVATION Elevation;
        DWORD cbSize = sizeof(TOKEN_ELEVATION);
        if (GetTokenInformation(hToken, TokenElevation, &Elevation, sizeof(Elevation), &cbSize)) {
            admin_priv = Elevation.TokenIsElevated;
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
        admin_priv = true;
#endif

<<<<<<< Updated upstream
=======
    if (adminPriv) {
        LOG_CRITICAL("PLEASE. DO NOT RUN RPCSV AS ADMIN OR WITH ADMIN PRIVILEGES.");
    }

>>>>>>> Stashed changes
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

    gui::i18n::apply_ui_language(app, cfg.user_lang);

#ifdef _WIN32
    {
        auto res = CoInitializeEx(NULL, COINIT_MULTITHREADED);
        LOG_ERROR_IF(res == S_FALSE, "Failed to initialize COM Library");
    }
#endif

    if (cfg.console) {
        if (logging::init(root_paths, false) != Success)
            return InitConfigFailed;
    } else {
        std::atexit(SDL_Quit);

        // Joystick events on background thread
        SDL_SetHint(SDL_HINT_JOYSTICK_THREAD, "1");
        // Enable HIDAPI rumble for DS4/DS
        SDL_SetHint(SDL_HINT_JOYSTICK_ENHANCED_REPORTS, "1");
        // Enable Switch controller
        SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_SWITCH, "1");
        SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_JOY_CONS, "1");

        if (!SDL_Init(SDL_INIT_AUDIO | SDL_INIT_GAMEPAD | SDL_INIT_HAPTIC | SDL_INIT_SENSOR | SDL_INIT_CAMERA)) {
            LOG_ERROR("SDL initialisation failed: {}", SDL_GetError());
            QMessageBox::critical(nullptr, "Error", "SDL initialisation failed.");
            return SDLInitFailed;
        }
    }

    LOG_INFO("{}", window_title);
    LOG_INFO("OS: {}", CppCommon::Environment::OSVersion());
    LOG_INFO("CPU: {} | {} Threads | {} GHz", CppCommon::CPU::Architecture(), CppCommon::CPU::LogicalCores(), static_cast<float>(CppCommon::CPU::ClockSpeed()) / 1000.f);
    LOG_INFO("Available ram memory: {} MiB", SDL_GetSystemRAM());

    app::AppRunType run_type = app::AppRunType::Unknown;
    if (cfg.run_app_path)
        run_type = app::AppRunType::Extracted;

    if (!app::init(emuenv, cfg, root_paths)) {
        QMessageBox::critical(nullptr, "Error", "Emulated environment initialization failed.");
        return 1;
    }

    if (emuenv.cfg.controller_binds.empty() || (emuenv.cfg.controller_binds.size() != 15)
        || emuenv.cfg.controller_axis_binds.empty() || (emuenv.cfg.controller_axis_binds.size() != 6))
        app::reset_controller_binding(emuenv);

    init_libraries(emuenv);

<<<<<<< Updated upstream
    if (!app::init_apps_list(emuenv)) {
        LOG_ERROR("Failed to initialize apps list.");
=======
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
            run_execv(argv, emuenv);
        }
        gui::init(gui, emuenv);
        app::update_viewport(emuenv);
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
    emuenv.io.addcont = APP_INDEX->addcont;
    emuenv.io.content_id = APP_INDEX->content_id;
    emuenv.io.savedata = APP_INDEX->savedata;
    emuenv.current_app_title = APP_INDEX->title;
    emuenv.app_info.app_short_title = APP_INDEX->stitle;
    emuenv.io.title_id = APP_INDEX->title_id;

    // Check license for PS App Only
    get_license(emuenv, emuenv.io.title_id, emuenv.io.content_id);

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
        app::error_dialog("Failed to initialize RPCSV", emuenv.window.get());
>>>>>>> Stashed changes
        return 1;
    }

    app::load_users(emuenv);

    if (cfg.content_path.has_value()) {
        const auto extension = string_utils::tolower(cfg.content_path->extension().string());
        const auto is_archive = (extension == ".vpk") || (extension == ".zip");
        const auto is_rif = (extension == ".rif") || (cfg.content_path->filename() == "work.bin");
        const auto is_directory = fs::is_directory(*cfg.content_path);

        std::string boot_title_id;

        if (is_archive) {
            LOG_INFO("Installing archive from CLI: {}", cfg.content_path->string());
            std::vector<ContentInfo> contents_info = install_archive(emuenv, *cfg.content_path);
            const auto content_index = std::find_if(contents_info.begin(), contents_info.end(), [](const ContentInfo &c) {
                return c.category == "gd";
            });
            if (content_index != contents_info.end() && content_index->state)
                boot_title_id = content_index->title_id;
        } else if (is_directory) {
            LOG_INFO("Installing contents from CLI: {}", cfg.content_path->string());
            if (install_contents(emuenv, *cfg.content_path) == 1 && emuenv.app_info.app_category == "gd")
                boot_title_id = emuenv.app_info.app_title_id;
        } else if (is_rif) {
            LOG_INFO("Installing license from CLI: {}", cfg.content_path->string());
            copy_license(emuenv, *cfg.content_path);
        } else {
            LOG_ERROR("File: [{}] is not a supported content type.", cfg.content_path->string());
        }

        cfg.content_path.reset();

        if (!boot_title_id.empty()) {
            cfg.run_app_path = boot_title_id;
            LOG_INFO("Content installed, will auto-boot: {}", boot_title_id);
        }

        if (!app::init_apps_list(emuenv)) {
            LOG_ERROR("Failed to refresh apps list after content install.");
        }
    }

    const QString gui_configs_dir = QString::fromStdString((emuenv.config_path / "gui-configs").string());
    auto gui_settings = std::make_shared<GuiSettings>(gui_configs_dir);
    auto persistent_settings = std::make_shared<PersistentSettings>(gui_configs_dir);

    MainWindow mainwindow(emuenv, gui_settings, persistent_settings, admin_priv);

    mainwindow.show();
    if (mainwindow.prompt_startup_warnings())
        app.exec();

#ifdef _WIN32
    CoUninitialize();
#endif

    return Success;
}
