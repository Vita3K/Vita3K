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

#include <gui-qt/gui_application.h>

#include <gui-qt/ctrl_keyboard_filter.h>
#include <gui-qt/game_window.h>
#include <gui-qt/gui_settings.h>
#include <gui-qt/ime_keyboard_filter.h>

#include <ctrl/functions.h>
#include <emuenv/app_launch_request.h>
#include <io/state.h>
#include <kernel/state.h>
#include <motion/event_handler.h>
#include <touch/functions.h>
#include <util/log.h>

#if USE_DISCORD
#include <app/discord.h>
#endif

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_gamepad.h>
#include <SDL3/SDL_sensor.h>

#include <QDataStream>
#include <QGuiApplication>
#include <QScreen>

GuiApplication::GuiApplication(EmuEnvState &emuenv,
    std::shared_ptr<GuiSettings> gui_settings,
    QObject *parent)
    : QObject(parent)
    , emuenv(emuenv)
    , m_app_session(emuenv)
    , m_gui_settings(std::move(gui_settings)) {
    m_sdl_pump_timer = new QTimer(this);
    m_sdl_pump_timer->setInterval(4);
    connect(m_sdl_pump_timer, &QTimer::timeout, this, &GuiApplication::pump_sdl_events);
    m_sdl_pump_timer->start();

    init_discord();
}

GuiApplication::~GuiApplication() {
    if (m_game_window)
        on_game_closed_internal(false);
    shutdown_discord();
}

bool GuiApplication::is_game_running() const {
    return m_game_window != nullptr;
}

bool GuiApplication::is_paused() const {
    return m_app_session.is_paused();
}

void GuiApplication::set_confirm_switch_callback(ConfirmSwitchCallback cb) {
    m_confirm_switch = std::move(cb);
}

void GuiApplication::set_confirm_firmware_callback(ConfirmFirmwareCallback cb) {
    m_confirm_firmware = std::move(cb);
}

void GuiApplication::set_sdl_event_filter(SdlEventFilter filter) {
    m_sdl_event_filter = std::move(filter);
}

void GuiApplication::boot_game(const std::string &title_id) {
    boot_game(AppLaunchRequest{
        .app_path = title_id,
    });
}

void GuiApplication::boot_game(const AppLaunchRequest &launch_request, const bool prompt_before_closing) {
    if (m_sdl_pump_timer)
        m_sdl_pump_timer->stop();

    AppLaunchRequest current_request = launch_request;
    bool should_prompt = prompt_before_closing;

    while (true) {
        auto next_request = boot_game_once(current_request, should_prompt);
        if (!next_request)
            break;

        current_request = std::move(*next_request);
        should_prompt = false;
    }

    if (m_sdl_pump_timer)
        m_sdl_pump_timer->start();
}

std::optional<AppLaunchRequest> GuiApplication::boot_game_once(const AppLaunchRequest &launch_request, const bool prompt_before_closing) {
    if (m_game_window) {
        if (prompt_before_closing) {
            if (m_confirm_switch && !m_confirm_switch())
                return std::nullopt;
        }

        on_game_closed_internal(false);
    }

    if (launch_request.reason != AppLaunchReason::LoadExec) {
        if (m_confirm_firmware && !m_confirm_firmware())
            return std::nullopt;
    }

    refresh_controllers(emuenv.ctrl, emuenv);
    emit controller_changed();

    const bool update_last_time_used = launch_request.reason != AppLaunchReason::LoadExec;
    if (!m_app_session.begin_launch(launch_request, update_last_time_used)) {
        emit boot_failed(tr("Could not find app '%1' in apps list.").arg(QString::fromStdString(launch_request.app_path)));
        return std::nullopt;
    }

    bool launch_fullscreen = emuenv.cfg.boot_apps_full_screen;

    m_game_window = new GameWindow(emuenv, m_gui_settings, emuenv.backend_renderer, QGuiApplication::primaryScreen());
    m_game_window->setTitle(tr("%1 (%2) | Loading...")
            .arg(QString::fromStdString(emuenv.current_app_title),
                QString::fromStdString(emuenv.io.title_id)));

    const QByteArray saved_geometry = m_gui_settings->get_value(gui::gw_geometry).toByteArray();
    if (!saved_geometry.isEmpty()) {
        QDataStream stream(saved_geometry);
        QRect rect;
        stream >> rect;
        if (rect.isValid())
            m_game_window->setGeometry(rect);
    }

    m_game_window->create();
    m_game_window->show();

    if (launch_fullscreen)
        m_game_window->showFullScreen();

    connect(m_game_window, &GameWindow::closed, this, [this]() {
        on_game_closed_internal();
    });

    const auto abort_boot = [&](const QString &msg) {
        emit boot_failed(msg);
        m_app_session.stop(app::AppSessionStopReason::LaunchFailure);

        if (m_game_window) {
            delete m_game_window;
            m_game_window = nullptr;
        }
    };

    if (emuenv.backend_renderer == renderer::Backend::OpenGL) {
        if (!m_game_window->create_gl_context()) {
            abort_boot(tr("Could not create OpenGL context.\nDoes your GPU support at least OpenGL 4.4?"));
            return std::nullopt;
        }
        if (!m_game_window->make_current()) {
            abort_boot(tr("Could not make OpenGL context current."));
            return std::nullopt;
        }
    }

    if (!m_app_session.initialize_renderer(*m_game_window)) {
        abort_boot(tr("Failed to initialise the renderer."));
        return std::nullopt;
    }

    m_game_window->setTitle(tr("%1 (%2) | Initializing...")
            .arg(QString::fromStdString(emuenv.current_app_title),
                QString::fromStdString(emuenv.io.title_id)));

    if (!m_app_session.initialize_runtime()) {
        abort_boot(tr("Failed to initialize emulator state."));
        return std::nullopt;
    }

    m_game_window->setTitle(tr("%1 (%2) | Loading modules...")
            .arg(QString::fromStdString(emuenv.current_app_title),
                QString::fromStdString(emuenv.io.title_id)));

    if (!m_app_session.load_and_run()) {
        abort_boot(tr("Failed to start game threads."));
        return std::nullopt;
    }

    install_event_filters(m_game_window);

    if (auto next_request = take_pending_app_launch_request()) {
        on_game_closed_internal(false);
        return next_request;
    }

    m_game_window->start_ui_updates();

    m_game_window->setTitle(tr("%1 (%2) | Please wait, loading...")
            .arg(QString::fromStdString(emuenv.current_app_title),
                QString::fromStdString(emuenv.io.title_id)));

    LOG_INFO("Game started: {} ({})", emuenv.current_app_title, launch_request.app_path);

    sync_discord_presence();
    emit game_started();
    return std::nullopt;
}

void GuiApplication::install_event_filters(GameWindow *window) {
    auto *kb_filter = new CtrlKeyboardFilter(emuenv, window);
    window->installEventFilter(kb_filter);
    m_kb_filter = kb_filter;

    auto *ime_filter = new ImeKeyboardFilter(emuenv, window);
    window->installEventFilter(ime_filter);

    connect(m_kb_filter, &CtrlKeyboardFilter::ps_button_pressed,
        this, [this]() {
            set_pause_reason(app::AppSessionPauseReason::User, !m_app_session.is_paused());
        });
}

void GuiApplication::on_game_closed_internal(bool emit_stopped) {
    if (!m_game_window)
        return;

    LOG_INFO("Game closed: {}", emuenv.current_app_title);

    if (m_gui_settings && m_game_window->visibility() != QWindow::FullScreen) {
        QByteArray data;
        QDataStream stream(&data, QIODevice::WriteOnly);
        stream << m_game_window->geometry();
        m_gui_settings->set_value(gui::gw_geometry, data, false);
    }

    m_game_window->stop_ui_updates();
    m_app_session.stop(m_is_shutting_down
            ? app::AppSessionStopReason::FrontendShutdown
            : app::AppSessionStopReason::UserRequest);
    refresh_controllers(emuenv.ctrl, emuenv);

    QWindow *closed_window = m_game_window;
    m_game_window = nullptr;
    m_kb_filter = nullptr;

    if (closed_window)
        delete closed_window;

    sync_discord_presence();
    if (emit_stopped)
        emit game_stopped();
}

void GuiApplication::stop_game() {
    if (!m_game_window)
        return;

    m_app_session.set_pause_reason(app::AppSessionPauseReason::User, false);
    on_game_closed_internal();
}

void GuiApplication::restart_game() {
    if (!m_game_window)
        return;

    AppLaunchRequest relaunch_request{
        .app_path = emuenv.io.app_path,
    };
    on_game_closed_internal(false);
    boot_game(relaunch_request, false);
}

void GuiApplication::set_pause_reason(app::AppSessionPauseReason reason, bool enabled) {
    if (!m_game_window)
        return;

    m_app_session.set_pause_reason(reason, enabled);
    emit pause_state_changed(m_app_session.is_paused());
}

std::optional<AppLaunchRequest> GuiApplication::take_pending_app_launch_request() {
    auto request = emuenv.take_app_launch_request();
    if (request)
        LOG_INFO("Handling in-process app relaunch for {} ({})", request->self_path, request->app_path);

    return request;
}

bool GuiApplication::handle_pending_app_launch_request() {
    if (!m_game_window)
        return false;

    auto request = take_pending_app_launch_request();
    if (!request)
        return false;

    const bool will_relaunch = request->reason != AppLaunchReason::ProcessExit;
    on_game_closed_internal(!will_relaunch);
    if (will_relaunch)
        boot_game(*request, false);
    return true;
}

void GuiApplication::pump_sdl_events() {
#if USE_DISCORD
    discordrpc::run_callbacks();
#endif

    if (handle_pending_app_launch_request())
        return;

    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
        case SDL_EVENT_GAMEPAD_ADDED:
        case SDL_EVENT_GAMEPAD_REMOVED:
            refresh_controllers(emuenv.ctrl, emuenv);
            emit controller_changed();
            break;

        case SDL_EVENT_GAMEPAD_AXIS_MOTION:
            if (m_sdl_event_filter && m_sdl_event_filter(event))
                break;
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
            if (m_sdl_event_filter && m_sdl_event_filter(event))
                break;
            if (event.gbutton.button == SDL_GAMEPAD_BUTTON_GUIDE)
                set_pause_reason(app::AppSessionPauseReason::User, !m_app_session.is_paused());
            if (m_game_window && !emuenv.kernel.is_threads_paused()
                && event.gbutton.button == SDL_GAMEPAD_BUTTON_TOUCHPAD)
                toggle_touchscreen(emuenv.touch);
            break;

        default:
            break;
        }
    }
}

void GuiApplication::init_discord() {
#if USE_DISCORD
    m_discord_rich_presence_old = emuenv.cfg.discord_rich_presence;
#endif
}

void GuiApplication::sync_discord_presence() {
#if USE_DISCORD
    if (!emuenv.cfg.discord_rich_presence) {
        if (m_discord_rich_presence_old)
            discordrpc::clear_presence();
        m_discord_rich_presence_old = false;
        return;
    }

    if (!discordrpc::init()) {
        m_discord_rich_presence_old = false;
        return;
    }

    if (m_game_window)
        discordrpc::update_presence(emuenv.io.title_id, emuenv.current_app_title);
    else
        discordrpc::update_presence();

    m_discord_rich_presence_old = true;
#endif
}

void GuiApplication::shutdown_discord() {
#if USE_DISCORD
    if (discordrpc::is_running())
        discordrpc::shutdown();
    m_discord_rich_presence_old = false;
#endif
}
