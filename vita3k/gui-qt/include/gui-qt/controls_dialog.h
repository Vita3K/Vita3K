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

#include <emuenv/state.h>
#include <input/physical_key.h>

#include <QDialog>
#include <QElapsedTimer>
#include <QTimer>

#include <array>
#include <memory>
#include <vector>

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_gamepad.h>

namespace Ui {
class ControlsDialog;
}

class GuiSettings;
class QCheckBox;
class QCloseEvent;
class QEvent;
class QGroupBox;
class QKeyEvent;
class QLabel;
class QListWidget;
class QListWidgetItem;
class QObject;
class QPushButton;
class QSlider;
class QStackedWidget;
class QToolButton;
class QWidget;

class ControlsDialog final : public QDialog {
    Q_OBJECT

public:
    explicit ControlsDialog(EmuEnvState &emuenv,
        std::shared_ptr<GuiSettings> gui_settings);
    ~ControlsDialog();

    static constexpr int MAX_CONTROLLERS = 4;

    void sync_controller_state();
    bool handle_sdl_event(const SDL_Event &event);

protected:
    void closeEvent(QCloseEvent *event) override;
    void done(int result) override;
    void keyPressEvent(QKeyEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    struct KeyboardBinding {
        QPushButton *button = nullptr;
        input::PhysicalKeyCode *config_value = nullptr;
    };

    struct KeyboardPage {
        QWidget *page = nullptr;
        QLabel *lbl_summary = nullptr;
        QStackedWidget *stack = nullptr;
        QToolButton *btn_primary = nullptr;
        QToolButton *btn_alt = nullptr;

        QPushButton *btn_dpad_up = nullptr, *btn_dpad_down = nullptr;
        QPushButton *btn_dpad_left = nullptr, *btn_dpad_right = nullptr;
        QPushButton *btn_cross = nullptr, *btn_circle = nullptr;
        QPushButton *btn_square = nullptr, *btn_triangle = nullptr;
        QPushButton *btn_l1 = nullptr, *btn_r1 = nullptr;
        QPushButton *btn_l2 = nullptr, *btn_r2 = nullptr;
        QPushButton *btn_l3 = nullptr, *btn_r3 = nullptr;
        QPushButton *btn_select = nullptr, *btn_start = nullptr, *btn_psbutton = nullptr;
        QPushButton *btn_lstick_up = nullptr, *btn_lstick_down = nullptr;
        QPushButton *btn_lstick_left = nullptr, *btn_lstick_right = nullptr;
        QPushButton *btn_rstick_up = nullptr, *btn_rstick_down = nullptr;
        QPushButton *btn_rstick_left = nullptr, *btn_rstick_right = nullptr;

        QPushButton *btn_dpad_up_alt = nullptr, *btn_dpad_down_alt = nullptr;
        QPushButton *btn_dpad_left_alt = nullptr, *btn_dpad_right_alt = nullptr;
        QPushButton *btn_cross_alt = nullptr, *btn_circle_alt = nullptr;
        QPushButton *btn_square_alt = nullptr, *btn_triangle_alt = nullptr;
        QPushButton *btn_l1_alt = nullptr, *btn_r1_alt = nullptr;
        QPushButton *btn_l2_alt = nullptr, *btn_r2_alt = nullptr;
        QPushButton *btn_l3_alt = nullptr, *btn_r3_alt = nullptr;
        QPushButton *btn_select_alt = nullptr, *btn_start_alt = nullptr, *btn_psbutton_alt = nullptr;
        QPushButton *btn_lstick_up_alt = nullptr, *btn_lstick_down_alt = nullptr;
        QPushButton *btn_lstick_left_alt = nullptr, *btn_lstick_right_alt = nullptr;
        QPushButton *btn_rstick_up_alt = nullptr, *btn_rstick_down_alt = nullptr;
        QPushButton *btn_rstick_left_alt = nullptr, *btn_rstick_right_alt = nullptr;

        QPushButton *btn_reset = nullptr;
        QLabel *l_controller_image = nullptr;
    };

    struct HotkeysPage {
        QWidget *page = nullptr;
        QLabel *lbl_summary = nullptr;
        QPushButton *btn_fullscreen = nullptr;
        QPushButton *btn_fullscreen_alt = nullptr;
        QPushButton *btn_toggle_touch = nullptr;
        QPushButton *btn_toggle_touch_alt = nullptr;
        QPushButton *btn_tex_replace = nullptr;
        QPushButton *btn_tex_replace_alt = nullptr;
        QPushButton *btn_screenshot = nullptr;
        QPushButton *btn_screenshot_alt = nullptr;
        QPushButton *btn_pinch_mod = nullptr;
        QPushButton *btn_pinch_mod_alt = nullptr;
        QPushButton *btn_alt_pinch_in = nullptr;
        QPushButton *btn_alt_pinch_in_alt = nullptr;
        QPushButton *btn_alt_pinch_out = nullptr;
        QPushButton *btn_alt_pinch_out_alt = nullptr;
        QPushButton *btn_reset = nullptr;
    };

    struct ControllerButtonBinding {
        QPushButton *button = nullptr;
        SDL_GamepadButton vita_button = SDL_GAMEPAD_BUTTON_INVALID;
    };

    struct ControllerAxisBinding {
        QPushButton *button = nullptr;
        int axis_index = -1;
    };

    struct ControllerPage {
        QWidget *page = nullptr;
        QLabel *lbl_summary = nullptr;
        QToolButton *btn_bindings = nullptr;
        QToolButton *btn_settings = nullptr;
        QStackedWidget *stack = nullptr;
        QLabel *l_controller_image = nullptr;

        QPushButton *btn_dpad_up = nullptr;
        QPushButton *btn_dpad_down = nullptr;
        QPushButton *btn_dpad_left = nullptr;
        QPushButton *btn_dpad_right = nullptr;
        QPushButton *btn_lstick_up = nullptr;
        QPushButton *btn_lstick_down = nullptr;
        QPushButton *btn_lstick_left = nullptr;
        QPushButton *btn_lstick_right = nullptr;
        QPushButton *btn_rstick_up = nullptr;
        QPushButton *btn_rstick_down = nullptr;
        QPushButton *btn_rstick_left = nullptr;
        QPushButton *btn_rstick_right = nullptr;
        QPushButton *btn_cross = nullptr;
        QPushButton *btn_circle = nullptr;
        QPushButton *btn_square = nullptr;
        QPushButton *btn_triangle = nullptr;
        QPushButton *btn_l1 = nullptr;
        QPushButton *btn_r1 = nullptr;
        QPushButton *btn_l2 = nullptr;
        QPushButton *btn_r2 = nullptr;
        QPushButton *btn_l3 = nullptr;
        QPushButton *btn_r3 = nullptr;
        QPushButton *btn_select = nullptr;
        QPushButton *btn_start = nullptr;
        QPushButton *btn_psbutton = nullptr;

        QSlider *slider_analog_multiplier = nullptr;
        QLabel *lbl_analog_multiplier_value = nullptr;
        QCheckBox *chk_disable_motion = nullptr;
        QPushButton *btn_reset = nullptr;

        std::vector<ControllerButtonBinding> bindings;
        std::vector<ControllerAxisBinding> axis_bindings;

        bool connected = false;
        int port = -1;
        SDL_GUID guid{};
        SDL_GamepadType type = SDL_GAMEPAD_TYPE_STANDARD;
    };

    void build_category_list();
    void build_keyboard_page();
    void build_hotkeys_page();
    void build_controller_page(int index);

    void refresh_shared_controller_settings();
    void set_analog_multiplier(float value);
    void set_motion_disabled(bool disabled);

    void init_keyboard_bindings();
    void refresh_all_keyboard_labels();
    void start_keyboard_capture(int index);
    void cancel_keyboard_capture();
    void reset_keyboard_defaults();
    bool has_duplicate_key(input::PhysicalKeyCode key, int exclude_index) const;
    void begin_capture_countdown();
    void refresh_capture_countdown();
    int capture_seconds_remaining() const;
    void stop_capture_countdown_if_idle();
    void set_keyboard_tab_view(bool show_primary);

    static QString key_name(input::PhysicalKeyCode key);

    void refresh_controller_tabs();
    void refresh_controller_page_labels(int index);
    void set_controller_page_enabled(int index, bool enabled);
    void set_controller_page_view(int index, bool show_bindings);
    void start_controller_capture(int tab_index, int binding_index);
    void start_axis_capture(int tab_index, int axis_binding_index);
    void cancel_controller_capture();
    void reset_controller_defaults(int index);

    void update_capture_countdown();

    static QString controller_button_name(SDL_GamepadType type, SDL_GamepadButton btn);
    static QString controller_axis_name(SDL_GamepadType type, SDL_GamepadAxis axis);

    void save_config();

    EmuEnvState &m_emuenv;
    std::shared_ptr<GuiSettings> m_gui_settings;
    std::unique_ptr<Ui::ControlsDialog> m_ui;

    KeyboardPage m_keyboard_page;
    HotkeysPage m_hotkeys_page;
    std::vector<KeyboardBinding> m_kb_bindings;
    int m_kb_capturing_index = -1;

    std::array<ControllerPage, MAX_CONTROLLERS> m_ctrl_tabs;
    int m_ctrl_capturing_tab = -1;
    int m_ctrl_capturing_index = -1;
    bool m_ctrl_capturing_axis = false;

    QListWidgetItem *m_keyboard_category_item = nullptr;
    QListWidgetItem *m_hotkeys_category_item = nullptr;
    std::array<QListWidgetItem *, MAX_CONTROLLERS> m_controller_category_items{};

    QTimer *m_capture_countdown_timer = nullptr;
    QElapsedTimer m_capture_timer;
    int m_last_capture_seconds_remaining = -1;
};
