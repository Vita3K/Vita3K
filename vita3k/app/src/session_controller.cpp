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

#include <app/functions.h>
#include <app/session_controller.h>

#include <audio/state.h>
#include <ctrl/state.h>
#include <display/state.h>
#include <gxm/functions.h>
#include <gxm/state.h>
#include <interface.h>
#include <io/state.h>
#include <kernel/state.h>
#include <motion/state.h>
#include <renderer/functions.h>
#include <util/log.h>

namespace app {

AppSessionController::AppSessionController(EmuEnvState &emuenv)
    : emuenv(emuenv) {
}

bool AppSessionController::has_active_session() const {
    return current_phase.load(std::memory_order_acquire) != AppSessionPhase::Idle;
}

bool AppSessionController::is_running() const {
    return current_phase.load(std::memory_order_acquire) == AppSessionPhase::Running;
}

bool AppSessionController::is_paused() const {
    return active_pause_reasons.load(std::memory_order_acquire) != 0;
}

bool AppSessionController::begin_launch(const AppLaunchRequest &launch_request, const bool update_last_time_used) {
    std::lock_guard<std::mutex> lock(mutex);
    const AppSessionPhase phase = current_phase.load(std::memory_order_relaxed);
    if (phase != AppSessionPhase::Idle) {
        LOG_ERROR("Refusing to begin launch for '{}' while session phase is '{}'.",
            launch_request.app_path, to_string(phase));
        return false;
    }

    if (!setup_game_launch(emuenv, launch_request.app_path, update_last_time_used))
        return false;

    active_launch_request = launch_request;
    renderer_initialized = false;
    runtime_initialized = false;
    active_pause_reasons.store(0, std::memory_order_release);
    input_intercepted = false;
    set_phase(AppSessionPhase::Launching);
    return true;
}

bool AppSessionController::initialize_renderer(renderer::FrameHost &frame) {
    std::lock_guard<std::mutex> lock(mutex);
    const AppSessionPhase phase = current_phase.load(std::memory_order_relaxed);
    if (phase != AppSessionPhase::Launching || renderer_initialized) {
        LOG_ERROR("Cannot initialize renderer while session phase is '{}'.", to_string(phase));
        return false;
    }

    frame_host = frame;

    if (!renderer::init(frame, emuenv.renderer,
            emuenv.backend_renderer, emuenv.cfg, emuenv.get_root_paths())) {
        return false;
    }

    apply_renderer_config(emuenv);
    renderer_initialized = true;
    return true;
}

bool AppSessionController::initialize_runtime() {
    std::lock_guard<std::mutex> lock(mutex);
    const AppSessionPhase phase = current_phase.load(std::memory_order_relaxed);
    if (phase != AppSessionPhase::Launching || !renderer_initialized || runtime_initialized) {
        LOG_ERROR("Cannot initialize runtime while session phase is '{}'.", to_string(phase));
        return false;
    }

    if (!late_init(emuenv))
        return false;

    runtime_initialized = true;
    return true;
}

bool AppSessionController::load_and_run() {
    std::lock_guard<std::mutex> lock(mutex);
    const AppSessionPhase phase = current_phase.load(std::memory_order_relaxed);
    if (phase != AppSessionPhase::Launching || !renderer_initialized || !runtime_initialized) {
        LOG_ERROR("Cannot start app while session phase is '{}'.", to_string(phase));
        return false;
    }

    emuenv.motion.refresh_device_motion_support();

    int32_t main_module_id = 0;
    if (load_app(main_module_id, emuenv, active_launch_request) != Success)
        return false;

    emuenv.renderer->set_app(emuenv.io.title_id.c_str(), emuenv.self_name.c_str());
    prepare_game_launch_overlay(emuenv);

    if (run_app(emuenv, main_module_id, active_launch_request) != Success)
        return false;

    frame_host->get().prepare_for_render_thread();

    renderer::start_render_thread(*emuenv.renderer, emuenv.display, emuenv.gxm, emuenv.mem, emuenv.cfg);

    frame_host->get().finalize_render_thread_start();

    set_phase(AppSessionPhase::Running);
    return true;
}

bool AppSessionController::set_pause_reason(const AppSessionPauseReason reason, const bool enabled) {
    std::lock_guard<std::mutex> lock(mutex);
    if (current_phase.load(std::memory_order_relaxed) != AppSessionPhase::Running)
        return false;

    const uint32_t mask = to_pause_mask(reason);
    const uint32_t current_reasons = active_pause_reasons.load(std::memory_order_relaxed);
    const uint32_t next_reasons = enabled
        ? (current_reasons | mask)
        : (current_reasons & ~mask);
    if (next_reasons == current_reasons)
        return true;

    active_pause_reasons.store(next_reasons, std::memory_order_release);
    apply_runtime_state_locked();
    return true;
}

bool AppSessionController::set_input_intercepted(const bool enabled) {
    std::lock_guard<std::mutex> lock(mutex);
    if (current_phase.load(std::memory_order_relaxed) != AppSessionPhase::Running)
        return false;

    if (input_intercepted == enabled)
        return true;

    input_intercepted = enabled;
    apply_runtime_state_locked();
    return true;
}

void AppSessionController::stop(const AppSessionStopReason reason) {
    std::optional<std::reference_wrapper<renderer::FrameHost>> active_frame_host;
    bool renderer_was_initialized = false;
    bool runtime_was_initialized = false;
    bool app_started = false;

    {
        std::lock_guard<std::mutex> lock(mutex);
        const AppSessionPhase phase = current_phase.load(std::memory_order_relaxed);
        if (phase == AppSessionPhase::Idle || phase == AppSessionPhase::Stopping)
            return;

        active_frame_host = frame_host;
        renderer_was_initialized = renderer_initialized || static_cast<bool>(emuenv.renderer);
        runtime_was_initialized = runtime_initialized;
        app_started = phase == AppSessionPhase::Running;
        set_phase(AppSessionPhase::Stopping);
    }

    const bool needs_renderer_cleanup = renderer_was_initialized || active_frame_host.has_value();

    if (runtime_was_initialized) {
        if (app_started && reason != AppSessionStopReason::LaunchFailure)
            update_app_time_used(emuenv, emuenv.io.app_path);

        shutdown_app_runtime(emuenv);
        reset_app_state(emuenv);
    } else if (renderer_was_initialized) {
        if (emuenv.renderer) {
            emuenv.renderer->cleanup();
            emuenv.renderer.reset();
        }
        abort_game_launch(emuenv);
    } else {
        abort_game_launch(emuenv);
    }

    if (needs_renderer_cleanup && active_frame_host)
        active_frame_host->get().destroy_render_context();

    emuenv.motion.clear_device_motion_support();

    {
        std::lock_guard<std::mutex> lock(mutex);
        reset_session_tracking();
    }
}

void AppSessionController::apply_runtime_state_locked() {
    const bool paused = active_pause_reasons.load(std::memory_order_relaxed) != 0;
    const bool currently_paused = emuenv.kernel.is_threads_paused();
    if (currently_paused != paused) {
        if (paused)
            emuenv.kernel.pause_threads();
        else
            emuenv.kernel.resume_threads();
    }

    if (emuenv.audio.adapter)
        emuenv.audio.switch_state(paused);

    emuenv.drop_inputs = paused;

    const bool overlay_intercepted = paused || input_intercepted;
    emuenv.ctrl.overlay_input_intercepted.store(overlay_intercepted, std::memory_order_relaxed);

    emuenv.renderer->paused.store(paused, std::memory_order_relaxed);
}

void AppSessionController::set_phase(const AppSessionPhase next_phase) {
    const AppSessionPhase current = current_phase.load(std::memory_order_relaxed);
    if (current == next_phase)
        return;

    LOG_DEBUG("App session phase: {} -> {}", to_string(current), to_string(next_phase));
    current_phase.store(next_phase, std::memory_order_release);
}

void AppSessionController::reset_session_tracking() {
    frame_host.reset();
    active_launch_request = {};
    active_pause_reasons.store(0, std::memory_order_release);
    renderer_initialized = false;
    runtime_initialized = false;
    input_intercepted = false;
    set_phase(AppSessionPhase::Idle);
}

const char *to_string(const AppSessionPhase phase) {
    switch (phase) {
    case AppSessionPhase::Idle:
        return "Idle";
    case AppSessionPhase::Launching:
        return "Launching";
    case AppSessionPhase::Running:
        return "Running";
    case AppSessionPhase::Stopping:
        return "Stopping";
    }

    return "Unknown";
}

uint32_t to_pause_mask(const AppSessionPauseReason reason) {
    return static_cast<uint32_t>(reason);
}

} // namespace app
