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

#include <gui-qt/no_gui_runner.h>

#include <gui-qt/ctrl_keyboard_filter.h>
#include <gui-qt/game_window.h>
#include <gui-qt/ime_keyboard_filter.h>

#include <app/functions.h>
#include <config/functions.h>
#include <ctrl/functions.h>
#include <ctrl/state.h>
#include <io/state.h>
#include <kernel/state.h>
#include <motion/event_handler.h>
#include <renderer/state.h>
#include <touch/functions.h>
#include <util/log.h>

#include <QCoreApplication>
#include <QString>
#include <QTimer>

#if USE_DISCORD
#include <app/discord.h>
#endif

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_gamepad.h>
#include <SDL3/SDL_sensor.h>

#include <utility>

NoGuiRunner::NoGuiRunner(EmuEnvState &emuenv, std::shared_ptr<GuiSettings> gui_settings, QObject *parent)
    : QObject(parent)
    , emuenv(emuenv)
    , m_app_session(emuenv)
    , m_gui_settings(std::move(gui_settings)) {
}

NoGuiRunner::~NoGuiRunner() {
    stop_session();
}

bool NoGuiRunner::start(const AppLaunchRequest &request) {
    ensure_active_user();

    if (!run_boot_chain(request))
        return false;

    m_sdl_pump_timer = new QTimer(this);
    m_sdl_pump_timer->setInterval(4);
    connect(m_sdl_pump_timer, &QTimer::timeout, this, &NoGuiRunner::pump_sdl_events);
    m_sdl_pump_timer->start();
    return true;
}

bool NoGuiRunner::run_boot_chain(AppLaunchRequest request) {
    while (true) {
        auto next = boot_once(request);
        if (!next)
            break;
        request = std::move(*next);
    }

    return m_game_window != nullptr;
}

std::optional<AppLaunchRequest> NoGuiRunner::boot_once(const AppLaunchRequest &request) {
    refresh_controllers(emuenv.ctrl, emuenv);

    const bool update_last_time_used = request.reason != AppLaunchReason::LoadExec;
    if (!m_app_session.begin_launch(request, update_last_time_used)) {
        LOG_ERROR("Failed to begin launch of '{}'", request.app_path);
        return std::nullopt;
    }

    const bool fullscreen = emuenv.cfg.fullscreen || emuenv.cfg.boot_apps_full_screen;

    m_game_window = new GameWindow(emuenv, m_gui_settings, emuenv.backend_renderer);
    m_game_window->setTitle(QString::fromStdString(emuenv.current_app_title));
    m_game_window->create();
    if (fullscreen)
        m_game_window->showFullScreen();
    else
        m_game_window->show();

    connect(m_game_window, &GameWindow::closed, this, &NoGuiRunner::on_game_closed);

    const auto abort = [this](const char *message) -> std::optional<AppLaunchRequest> {
        LOG_ERROR("{}", message);
        stop_session();
        return std::nullopt;
    };

    if (emuenv.backend_renderer == renderer::Backend::OpenGL) {
        if (!m_game_window->create_gl_context())
            return abort("Could not create OpenGL context");
        if (!m_game_window->make_current())
            return abort("Could not make OpenGL context current");
    }

    if (!m_app_session.initialize_renderer(*m_game_window))
        return abort("Failed to initialise the renderer");
    if (!m_app_session.initialize_runtime())
        return abort("Failed to initialise emulator state");
    if (!m_app_session.load_and_run())
        return abort("Failed to start game threads");

    auto *kb_filter = new CtrlKeyboardFilter(emuenv, m_game_window);
    m_game_window->installEventFilter(kb_filter);
    connect(kb_filter, &CtrlKeyboardFilter::fullscreen_toggled,
        this, [this]() { m_game_window->toggle_fullscreen(); });

    auto *ime_filter = new ImeKeyboardFilter(emuenv, m_game_window);
    m_game_window->installEventFilter(ime_filter);

    if (auto pending = emuenv.take_app_launch_request()) {
        stop_session();
        if (pending->reason == AppLaunchReason::ProcessExit)
            return std::nullopt;
        return pending;
    }

    m_game_window->start_ui_updates();
    LOG_INFO("Game started: {} ({})", emuenv.current_app_title, request.app_path);
    return std::nullopt;
}

void NoGuiRunner::pump_sdl_events() {
#if USE_DISCORD
    discordrpc::run_callbacks();
#endif

    if (auto request = emuenv.take_app_launch_request()) {
        LOG_INFO("Handling in-process app relaunch for {} ({})", request->self_path, request->app_path);
        stop_session();

        if (request->reason == AppLaunchReason::ProcessExit) {
            QCoreApplication::quit();
            return;
        }

        if (!run_boot_chain(std::move(*request)))
            QCoreApplication::quit();
        return;
    }

    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
        case SDL_EVENT_GAMEPAD_ADDED:
        case SDL_EVENT_GAMEPAD_REMOVED:
            refresh_controllers(emuenv.ctrl, emuenv);
            break;

        case SDL_EVENT_GAMEPAD_TOUCHPAD_DOWN:
        case SDL_EVENT_GAMEPAD_TOUCHPAD_MOTION:
        case SDL_EVENT_GAMEPAD_TOUCHPAD_UP:
            handle_touchpad_event(emuenv.touch, event.gtouchpad);
            break;

        case SDL_EVENT_GAMEPAD_SENSOR_UPDATE:
            handle_motion_event(emuenv, event.gsensor.sensor, event.gsensor);
            break;

        case SDL_EVENT_SENSOR_UPDATE:
            handle_motion_event(emuenv, SDL_GetSensorTypeForID(event.sensor.which), event.sensor);
            break;

        case SDL_EVENT_FINGER_DOWN:
        case SDL_EVENT_FINGER_MOTION:
        case SDL_EVENT_FINGER_UP:
            handle_touch_event(emuenv.touch, event.tfinger);
            break;

        case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
            if (m_game_window && !emuenv.kernel.is_threads_paused()
                && event.gbutton.button == SDL_GAMEPAD_BUTTON_TOUCHPAD)
                toggle_touchscreen(emuenv.touch);
            break;

        default:
            break;
        }
    }
}

void NoGuiRunner::on_game_closed() {
    stop_session();
    QCoreApplication::quit();
}

void NoGuiRunner::stop_session() {
    if (!m_game_window)
        return;

    LOG_INFO("Game closed: {}", emuenv.current_app_title);

    m_game_window->stop_ui_updates();
    m_app_session.stop(app::AppSessionStopReason::FrontendShutdown);
    refresh_controllers(emuenv.ctrl, emuenv);

    GameWindow *closed_window = m_game_window;
    m_game_window = nullptr;
    delete closed_window;
}

void NoGuiRunner::ensure_active_user() {
    const auto &users = emuenv.app.user_list.users;

    if (users.empty()) {
        const std::string id = app::create_user(emuenv, "Vita3K");
        app::activate_user(emuenv, id);
        emuenv.cfg.user_id = id;
        config::serialize_config(emuenv.cfg, emuenv.cfg.config_path);
        return;
    }

    if (!emuenv.cfg.user_id.empty() && users.contains(emuenv.cfg.user_id)) {
        app::activate_user(emuenv, emuenv.cfg.user_id);
        return;
    }

    const auto &first = users.begin();
    app::activate_user(emuenv, first->first);
    emuenv.cfg.user_id = first->first;
    config::serialize_config(emuenv.cfg, emuenv.cfg.config_path);
}
