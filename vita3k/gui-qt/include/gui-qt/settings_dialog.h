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

#include <app/functions.h>
#include <config/state.h>

#include <QDialog>
#include <QDialogButtonBox>

#include <memory>
#include <string>
#include <vector>

struct EmuEnvState;
class GuiSettings;
class SettingsDialogTooltips;
class ThemeManager;

enum class SettingsTab : int {
    Core = 0,
    CPU,
    Graphics,
    Audio,
    Camera,
    System,
    Emulator,
    Interface,
    Network,
    Debug,
};

namespace Ui {
class vita3k_settings_dialog;
}

class SettingsDialog final : public QDialog {
    Q_OBJECT

public:
    explicit SettingsDialog(EmuEnvState &emuenv,
        std::shared_ptr<GuiSettings> gui_settings,
        ThemeManager &theme_manager,
        const std::string &app_path = {},
        int initial_tab = 0);
    ~SettingsDialog();

    void show_tab(int tab_index);
    void set_storage_path_locked(bool locked);

    bool storage_path_switched() const {
        return m_storage_path_switched;
    }

    void reject() override;

Q_SIGNALS:
    void gui_stylesheet_request();
    void gui_theme_music_settings_request();
    void gui_log_settings_request();
    void storage_path_changed();
    void ui_language_request(const QString &locale_tag);
    void restart_game_requested();

private slots:
    void on_save_clicked();
    void on_apply_clicked();
    void on_close_clicked();
    void refresh_themes();

private:
    bool apply_pending_storage_path();
    bool commit_changes(bool close_after);
    bool prompt_restart_if_needed(const app::SettingsCommitResult &result, bool close_after);
    void apply_text_overrides();
    void load_config();
    void build_desired_config(Config &desired) const;
    void populate_modules_list();
    void populate_tracy_modules_list();
    void populate_cameras_list();
    void populate_adhoc_list();
    void setup_connections();
    void setup_dirty_tracking();
    void update_resolution_label();
    void update_aniso_label();
    void update_gpu_visibility();
    void update_camera_widgets();
    void update_camera_color_preview();
    void update_current_emu_path_label();
    void update_modules_list_enabled();
    void update_file_loading_delay_label();
    void update_http_retry_labels();
    void mark_dirty();
    void set_pending_vita_fs_path(const fs::path &vita_fs_path);
    void set_description(QWidget *tab, const QString &title, const QString &text);
    void add_stylesheets(const QString &preferred_name = {});
    void apply_stylesheet(bool reset = false);

    void closeEvent(QCloseEvent *event) override;
    void done(int result) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

    std::vector<QDialogButtonBox *> m_button_boxes;

    EmuEnvState &emuenv;
    std::shared_ptr<GuiSettings> m_gui_settings;
    ThemeManager &m_theme_manager;
    std::unique_ptr<SettingsDialogTooltips> m_tooltips;
    std::string m_app_path;
    int m_initial_tab;
    bool m_dirty = false;
    bool m_storage_path_locked = false;
    bool m_storage_path_switched = false;
    Config::CurrentConfig m_config;
    fs::path m_pending_vita_fs_path{};
    std::vector<config::RestartRequiredSetting> m_deferred_restart_settings;
    std::unique_ptr<Ui::vita3k_settings_dialog> m_ui;
    QString m_current_stylesheet;
};
