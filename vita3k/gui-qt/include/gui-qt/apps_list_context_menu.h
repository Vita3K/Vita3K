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

#pragma once

#include <app/state.h>
#include <util/fs.h>

#include <QMenu>

#include <vector>

struct EmuEnvState;
class GameCompatibility;

class AppsListContextMenu : public QMenu {
    Q_OBJECT

public:
    explicit AppsListContextMenu(EmuEnvState &emuenv,
        const std::vector<const app::AppEntry *> &apps,
        GameCompatibility *compat,
        bool app_running,
        QWidget *parent = nullptr);

Q_SIGNALS:
    void boot_requested(const app::AppEntry &app);
    void view_live_area_requested(const app::AppEntry &app);
    void open_settings_requested(const std::string &app_path, int tab);
    void refresh_requested();

private:
    struct AppPaths {
        fs::path app;
        fs::path save_data;
        fs::path patch;
        fs::path addcont;
        fs::path license;
        fs::path shader_cache;
        fs::path shader_log;
        fs::path shader_log_alt;
        fs::path export_textures;
        fs::path import_textures;
    };

    AppPaths make_app_paths(const app::AppEntry &app) const;

    void build_single(const app::AppEntry &app);
    void build_multi(const std::vector<const app::AppEntry *> &apps);

    void add_boot_actions(const app::AppEntry &app);
    void add_compat_actions(const app::AppEntry &app);
    void add_copy_info_actions(const app::AppEntry &app);
    void add_custom_config_actions(const app::AppEntry &app);
    void add_open_folder_actions(const app::AppEntry &app);
    void add_delete_actions(const app::AppEntry &app);
    void add_misc_actions(const app::AppEntry &app);
    void add_information_actions(const app::AppEntry &app);

    EmuEnvState &m_emuenv;
    GameCompatibility *m_compat;
    AppPaths m_paths;
    bool m_app_running;
};
