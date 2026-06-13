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

#include <memory>
#include <optional>

class GameWindow;
class GuiSettings;
class QTimer;

class NoGuiRunner : public QObject {
    Q_OBJECT

public:
    NoGuiRunner(EmuEnvState &emuenv, std::shared_ptr<GuiSettings> gui_settings, QObject *parent = nullptr);
    ~NoGuiRunner() override;

    bool start(const AppLaunchRequest &request);

private slots:
    void pump_sdl_events();
    void on_game_closed();

private:
    bool run_boot_chain(AppLaunchRequest request);
    std::optional<AppLaunchRequest> boot_once(const AppLaunchRequest &request);
    void stop_session();
    void ensure_active_user();

    EmuEnvState &emuenv;
    app::AppSessionController m_app_session;
    std::shared_ptr<GuiSettings> m_gui_settings;
    GameWindow *m_game_window = nullptr;
    QTimer *m_sdl_pump_timer = nullptr;
};
