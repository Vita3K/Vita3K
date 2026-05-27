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
#include <emuenv/state.h>

#include <QMainWindow>
#include <QPixmap>
#include <QPointer>
#include <QTimer>

#include <memory>
#include <optional>
#include <vector>

class GuiApplication;
class GuiSettings;
class PersistentSettings;

namespace Ui {
class MainWindow;
}

class AppsList;
class LogWidget;
class GameWindow;
class GameCompatibility;
class QWindow;
class QWidget;
class QLineEdit;
class QPaintEvent;
class QPushButton;
class QSlider;
class QToolBar;
class QVariantAnimation;
class TrophyCollectionDialog;
class CtrlKeyboardFilter;
class DebugWidget;
class LiveAreaWidget;
class UserManagementDialog;
class UpdateManager;
class ControlsDialog;
class SettingsDialog;
class ThemeManager;
class VitaThemesDialog;
namespace app {
struct SettingsCommitResult;
}

class MainWindow final : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(EmuEnvState &emuenv,
        std::shared_ptr<GuiSettings> gui_settings,
        std::shared_ptr<PersistentSettings> persistent_settings,
        GuiApplication *gui_app,
        bool admin_privileged = false);
    ~MainWindow();

    LogWidget *log_widget() const { return m_log_widget; }
    bool prompt_startup_warnings();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

    void changeEvent(QEvent *event) override;
    void closeEvent(QCloseEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

private slots:
    void on_about_action_triggered();
    void on_about_qt_action_triggered();
    void on_controls_action_triggered();
    void on_check_for_updates_action_triggered();
    void on_app_selected(const app::AppEntry &app);
    void open_settings(int tab_index = 0);
    void apply_ui_language(const QString &locale_tag);
    void on_game_started();
    void on_game_stopped();
    void on_boot_failed(const QString &message);
    void on_controller_changed();
    void restart_running_app();
    void open_trophy_collection();
    void open_user_management();
    void open_vita_themes();
    void show_live_area(const std::string &title_id);
    void on_live_area_play();
    void on_live_area_closed();

    void on_install_firmware_triggered();
    void on_install_pkg_triggered();
    void on_install_zip_triggered();
    void on_install_license_triggered();

    void on_install_finished();

    void on_pause_triggered();
    void on_stop_triggered();
    void on_ps_button();
    void on_toolbar_refresh();
    void on_toolbar_fullscreen();
    void on_toolbar_start();
    void on_app_selection_changed(const app::AppEntry *app);
    void on_context_menu_requested(const QPoint &global_pos, const std::vector<const app::AppEntry *> &apps);
    void resize_icons(int index);

private:
    void refresh_emulation_actions();
    void initialize();
    void init_current_user();
    bool confirm_missing_firmware_warning();
    static QImage load_app_background(const std::string &app_path,
        const std::string &pref_path);

    void handle_drop(const QString &path);

    void setup_toolbar();
    void repaint_toolbar_icons();
    void setup_status_bar();
    void show_title_bars(bool show) const;
    void update_renderer_button();
    void update_accuracy_button();
    void update_screen_filter_button();
    void update_audio_backend_button();
    void update_ngs_button();
    void update_volume_button();
    void update_update_available_button();
    void refresh_status_bar();
    void save_config();
    void save_config(const Config &desired_cfg);
    void prompt_restart_if_needed(const app::SettingsCommitResult &result);
    bool prompt_admin_privileges_warning_if_needed();
    void register_auxiliary_window(QWidget *window);
    bool close_auxiliary_windows();
    void prune_auxiliary_windows();
    void save_window_state();
    void restore_window_state();
    void open_debug_widget(int tab);
    void apply_log_gui_settings();
    void repaint_gui();
    void init_first_run_stylesheet();
    void apply_stylesheet();
    void apply_theme_background_presentation();
    void clear_theme_background_presentation();
    void rescale_theme_background_pixmaps();
    void advance_theme_background_presentation();

    EmuEnvState &emuenv;
    GuiApplication *m_gui_app = nullptr;
    std::unique_ptr<Ui::MainWindow> m_ui;
    std::shared_ptr<GuiSettings> m_gui_settings;
    std::shared_ptr<PersistentSettings> m_persistent_settings;
    std::unique_ptr<ThemeManager> m_theme_manager;

    QMainWindow *m_dock_host = nullptr;
    AppsList *m_apps_list_widget = nullptr;
    LogWidget *m_log_widget = nullptr;
    GameCompatibility *m_game_compat = nullptr;
    QWindow *m_game_container = nullptr;

    QPointer<TrophyCollectionDialog> m_trophy_dialog;
    QPointer<UserManagementDialog> m_user_mgmt_dialog;
    QPointer<DebugWidget> m_debug_widget;
    LiveAreaWidget *m_live_area_widget = nullptr;
    UpdateManager *m_update_manager = nullptr;
    QPointer<ControlsDialog> m_controls_dialog;
    QPointer<SettingsDialog> m_settings_dialog;
    QPointer<VitaThemesDialog> m_vita_themes_dialog;
    std::string m_live_area_title_id;

    QLineEdit *m_search_bar = nullptr;
    QSlider *m_icon_size_slider = nullptr;
    QWidget *m_slider_container = nullptr;
    std::vector<QPointer<QWidget>> m_auxiliary_windows;
    QTimer *m_theme_background_cycle_timer = nullptr;
    QVariantAnimation *m_theme_background_fade_animation = nullptr;
    std::vector<QPixmap> m_theme_background_original_pixmaps;
    std::vector<QPixmap> m_theme_background_scaled_pixmaps;
    QSize m_theme_background_scaled_size;
    int m_theme_background_cycle_interval_ms = 15000;
    int m_theme_background_current_index = 0;
    int m_theme_background_next_index = -1;
    qreal m_theme_background_fade_progress = 0.0;
    bool m_is_app_closing = false;
    bool m_fullscreen = false;
    bool m_app_selected = false;
    bool m_admin_privileged = false;

    QIcon m_icon_play;
    QIcon m_icon_pause;
    QIcon m_icon_stop;
    QIcon m_icon_fullscreen_on;
    QIcon m_icon_fullscreen_off;

    QPushButton *m_renderer_button = nullptr;
    QPushButton *m_accuracy_button = nullptr;
    QPushButton *m_screen_filter_button = nullptr;
    QPushButton *m_audio_backend_button = nullptr;
    QPushButton *m_ngs_button = nullptr;
    QPushButton *m_volume_button = nullptr;
    QPushButton *m_update_available_button = nullptr;
    bool m_update_available = false;
};
