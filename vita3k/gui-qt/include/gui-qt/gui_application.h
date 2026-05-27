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

#include <app/session_controller.h>
#include <emuenv/state.h>

#include <QObject>
#include <QTimer>

#include <SDL3/SDL_events.h>

#include <functional>
#include <memory>
#include <optional>

class GameWindow;
class GuiSettings;
class CtrlKeyboardFilter;
struct AppLaunchRequest;

class GuiApplication final : public QObject {
    Q_OBJECT

public:
    explicit GuiApplication(EmuEnvState &emuenv,
        std::shared_ptr<GuiSettings> gui_settings,
        QObject *parent = nullptr);
    ~GuiApplication();

    void boot_game(const std::string &title_id);
    void boot_game(const AppLaunchRequest &request, bool prompt_before_closing = true);
    void stop_game();
    void restart_game();
    void set_pause_reason(app::AppSessionPauseReason reason, bool enabled);

    bool is_game_running() const;
    bool is_paused() const;
    GameWindow *game_window() const { return m_game_window; }
    CtrlKeyboardFilter *keyboard_filter() const { return m_kb_filter; }
    app::AppSessionController &session() { return m_app_session; }

    using ConfirmSwitchCallback = std::function<bool()>;
    using ConfirmFirmwareCallback = std::function<bool()>;
    using SdlEventFilter = std::function<bool(const SDL_Event &)>;
    void set_confirm_switch_callback(ConfirmSwitchCallback cb);
    void set_confirm_firmware_callback(ConfirmFirmwareCallback cb);
    void set_sdl_event_filter(SdlEventFilter filter);

signals:
    void game_started();
    void game_stopped();
    void pause_state_changed(bool paused);
    void boot_failed(const QString &message);
    void controller_changed();

private slots:
    void pump_sdl_events();

private:
    std::optional<AppLaunchRequest> boot_game_once(const AppLaunchRequest &request, bool prompt_before_closing);
    std::optional<AppLaunchRequest> take_pending_app_launch_request();
    bool handle_pending_app_launch_request();
    void on_game_closed_internal(bool emit_stopped = true);
    void install_event_filters(GameWindow *window);

    void init_discord();
    void sync_discord_presence();
    void shutdown_discord();

    EmuEnvState &emuenv;
    app::AppSessionController m_app_session;
    std::shared_ptr<GuiSettings> m_gui_settings;
    GameWindow *m_game_window = nullptr;
    QTimer *m_sdl_pump_timer = nullptr;
    CtrlKeyboardFilter *m_kb_filter = nullptr;
    bool m_is_shutting_down = false;

    ConfirmSwitchCallback m_confirm_switch;
    ConfirmFirmwareCallback m_confirm_firmware;
    SdlEventFilter m_sdl_event_filter;

#if USE_DISCORD
    bool m_discord_rich_presence_old = false;
#endif
};
