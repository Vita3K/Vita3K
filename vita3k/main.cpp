// Vita3K emulator project
// Copyright (C) 2026 Vita3K team
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
#ifdef HAS_QT
#include <gui-qt/gui_language.h>
#include <gui-qt/gui_settings.h>
#include <gui-qt/log_widget.h>
#include <gui-qt/main_window.h>
#include <gui-qt/persistent_settings.h>
#endif
#include <io/state.h>
#include <modules/module_parent.h>
#include <packages/functions.h>
#include <packages/license.h>
#include <packages/pkg.h>
#include <packages/sfo.h>
#include <sdl-frontend/functions.h>
#include <sdl/functions.h>
#include <shader/spirv_recompiler.h>
#include <util/log.h>
#include <util/string_utils.h>
#include <util/sysinfo.h>

#ifdef HAS_QT
#include <QApplication>
#include <QMessageBox>
#include <QSysInfo>
#endif

#if USE_DISCORD
#include <app/discord.h>
#endif

#if defined(_WIN32) && defined(HAS_QT)
#include <combaseapi.h>
#define SDL_MAIN_HANDLED
#elif defined(_WIN32)
#include <combaseapi.h>
#endif

#ifdef TRACY_ENABLE
#include <tracy/Tracy.hpp>
#endif

#ifdef HAS_QT
#include "gui-qt/qt_utils.h"
#endif

#include <SDL3/SDL_cpuinfo.h>
#include <SDL3/SDL_gamepad.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_platform.h>

#include <cstdlib>
#include <optional>

int main(int argc, char *argv[]) {
#ifdef HAS_QT
#ifdef __APPLE__
    qputenv("QT_MTL_NO_TRANSACTION", "1");
    qputenv("QT_MAC_NO_CONTAINER_LAYER", "1");
#endif

    QCoreApplication::setOrganizationName(QStringLiteral("Vita3K"));
    QCoreApplication::setApplicationName(QStringLiteral("Vita3K"));
#endif

#ifdef TRACY_ENABLE
    ZoneScoped; // Tracy - Track main function scope
#endif

    Root root_paths;
    bool portable = app::init_paths(root_paths);

    if (!fs::exists(root_paths.get_vita_fs_path())) {
        fs::create_directories(root_paths.get_vita_fs_path());
    }

    // Check admin privs before init starts to avoid creating of file as other user by accident
    bool admin_priv = false;
#ifdef _WIN32
    // https://stackoverflow.com/questions/8046097/how-to-check-if-a-process-has-the-administrative-rights
    HANDLE hToken = nullptr;
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

    Config cfg{};
    EmuEnvState emuenv;
    const auto config_err = config::init_config(cfg, argc, argv, root_paths, portable);

#ifdef HAS_QT
    const bool is_sdl_frontend = cfg.frontend == Frontend::sdl;
    std::optional<QApplication> qt_app;
    if (!is_sdl_frontend) {
        qt_app.emplace(argc, argv);
        gui::i18n::apply_ui_language(*qt_app, cfg.user_lang, emuenv.static_assets_path);
    }
#else
    const bool is_sdl_frontend = true;
#endif

#ifdef HAS_QT
    if (qt_app)
        LogWidget::register_callback();
#endif

#if defined(_WIN32) && !defined(__ANDROID__)
    if (is_sdl_frontend)
        sdl::init_console();
#endif
    if (logging::init(root_paths, true) != Success)
        return InitConfigFailed;

    if (config_err != Success) {
        if (config_err == QuitRequested) {
            if (cfg.recompile_shader_path.has_value()) {
                LOG_INFO("Recompiling {}", *cfg.recompile_shader_path);
                shader::convert_gxp_to_glsl_from_filepath(*cfg.recompile_shader_path);
            }
            if (cfg.delete_title_id.has_value()) {
                LOG_INFO("Deleting title id {}", *cfg.delete_title_id);
                fs::remove_all(cfg.get_vita_fs_path() / "ux0/app" / *cfg.delete_title_id);
                fs::remove_all(cfg.get_vita_fs_path() / "ux0/addcont" / *cfg.delete_title_id);
                fs::remove_all(cfg.get_vita_fs_path() / "ux0/user/00/savedata" / *cfg.delete_title_id);
                fs::remove_all(root_paths.get_cache_path() / "shaders" / *cfg.delete_title_id);
            }
            if (cfg.pup_path.has_value()) {
                fs::create_directories(cfg.get_vita_fs_path());
                LOG_INFO("Installing firmware file {}", *cfg.pup_path);
                install_pup(cfg.get_vita_fs_path(), *cfg.pup_path, [](uint32_t progress) {
                    LOG_INFO("Firmware installation progress: {}%", progress);
                });
            }
            if (cfg.pkg_path.has_value() && cfg.pkg_zrif.has_value()) {
                fs::create_directories(cfg.get_vita_fs_path());
                LOG_INFO("Installing pkg from {} ", *cfg.pkg_path);
                emuenv.cache_path = root_paths.get_cache_path();
                emuenv.vita_fs_path = cfg.get_vita_fs_path();
                auto pkg_path = fs_utils::utf8_to_path(*cfg.pkg_path);
                install_pkg(pkg_path, emuenv, *cfg.pkg_zrif, [](float) {});
            }
            return Success;
        }
        LOG_ERROR("Failed to initialise config");
        return InitConfigFailed;
    }

    fs::create_directories(cfg.get_vita_fs_path());

#ifdef _WIN32
    {
        auto res = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        LOG_ERROR_IF(res == S_FALSE, "Failed to initialize COM Library");
    }
#endif

    if (cfg.console) {
        if (logging::init(root_paths, false) != Success)
            return InitConfigFailed;
    } else {
        std::atexit(SDL_Quit);
        if (!sdl::initialize(is_sdl_frontend)) {
            LOG_ERROR("SDL initialisation failed: {}", SDL_GetError());
#ifdef HAS_QT
            if (qt_app)
                QMessageBox::critical(nullptr, "Error", "SDL initialisation failed.");
#endif
            return SDLInitFailed;
        }
    }

    LOG_INFO("{}", window_title);
#ifdef HAS_QT
    LOG_INFO("OS: {}", QSysInfo::prettyProductName().toStdString());
#else
    LOG_INFO("OS: {}", SDL_GetPlatform());
#endif
    LOG_INFO("CPU: {}", util::get_system_info());
    LOG_INFO("Available ram memory: {} MiB", SDL_GetSystemRAM());

    if (!app::init(emuenv, cfg, root_paths)) {
        LOG_ERROR("Emulated environment initialization failed.");
#ifdef HAS_QT
        if (qt_app)
            QMessageBox::critical(nullptr, "Error", "Emulated environment initialization failed.");
#endif
        return 1;
    }

    init_libraries(emuenv);

    if (!app::init_apps_list(emuenv)) {
        LOG_ERROR("Failed to initialize apps list.");
        return 1;
    }

    app::load_users(emuenv);

    if (emuenv.cfg.content_path.has_value()) {
        const auto extension = string_utils::tolower(emuenv.cfg.content_path->extension().string());
        const auto is_archive = (extension == ".vpk") || (extension == ".zip");
        const auto is_rif = (extension == ".rif") || (emuenv.cfg.content_path->filename() == "work.bin");
        const auto is_directory = fs::is_directory(*emuenv.cfg.content_path);

        std::string boot_title_id;

        if (is_archive) {
            LOG_INFO("Installing archive from CLI: {}", emuenv.cfg.content_path->string());
            std::vector<ContentInfo> contents_info = install_archive(emuenv, *emuenv.cfg.content_path);
            const auto content_index = std::ranges::find_if(contents_info, [](const ContentInfo &c) {
                return c.category == "gd";
            });
            if (content_index != contents_info.end() && content_index->state)
                boot_title_id = content_index->title_id;
        } else if (is_directory) {
            LOG_INFO("Installing contents from CLI: {}", emuenv.cfg.content_path->string());
            if (install_contents(emuenv, *emuenv.cfg.content_path) == 1 && emuenv.app_info.app_category == "gd")
                boot_title_id = emuenv.app_info.app_title_id;
        } else if (is_rif) {
            LOG_INFO("Installing license from CLI: {}", emuenv.cfg.content_path->string());
            copy_license(emuenv, *emuenv.cfg.content_path);
        } else {
            LOG_ERROR("File: [{}] is not a supported content type.", emuenv.cfg.content_path->string());
        }

        emuenv.cfg.content_path.reset();

        if (!boot_title_id.empty()) {
            emuenv.cfg.run_app_path = boot_title_id;
            LOG_INFO("Content installed, will auto-boot: {}", boot_title_id);
        }

        if (!app::init_apps_list(emuenv)) {
            LOG_ERROR("Failed to refresh apps list after content install.");
        }
    }

    if (is_sdl_frontend) {
        const int exit_code = sdl_frontend::run_frontend(emuenv);
#ifdef _WIN32
        CoUninitialize();
#endif
        return exit_code;
    }

#ifdef HAS_QT
    const QString gui_configs_dir = gui::utils::to_qt_path(emuenv.config_path / "gui-configs");
    auto gui_settings = std::make_shared<GuiSettings>(gui_configs_dir, emuenv.static_assets_path / "data" / "gui-configs" / "custom-themes");
    auto persistent_settings = std::make_shared<PersistentSettings>(gui_configs_dir);

    MainWindow mainwindow(emuenv, gui_settings, persistent_settings, admin_priv);

    mainwindow.show();
    if (mainwindow.prompt_startup_warnings())
        qt_app->exec();
#endif

#ifdef _WIN32
    CoUninitialize();
#endif

    return Success;
}
