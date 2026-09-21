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

#include <display/functions.h>

#include <dialog/state.h>
#include <display/state.h>
#include <emuenv/state.h>
#include <kernel/state.h>
#include <renderer/state.h>

#include <chrono>
#include <motion/functions.h>
#include <touch/functions.h>

// Code heavily influenced by PPSSSPP's SceDisplay.cpp

static constexpr int TARGET_FPS = 60;
static constexpr int64_t TARGET_MICRO_PER_FRAME = 1000000LL / TARGET_FPS;
// how many cycles do we need to see before we start predicting the next frame
static constexpr int predict_threshold = 3;
static constexpr int max_expected_swapchain_size = 6;

static void run_vblank_tick(EmuEnvState &emuenv) {
    DisplayState &display = emuenv.display;
    std::vector<CallbackPtr> callbacks_to_notify;

    {
        const std::lock_guard<std::mutex> guard_info(display.display_info_mutex);
        ++display.vblank_count;

        // in this case, even though no new game frames are being rendered, we still need to update the screen
        if (emuenv.kernel.is_threads_paused() || (emuenv.common_dialog.status == SCE_COMMON_DIALOG_STATUS_RUNNING))
            // only display the UI/common dialog at 30 fps
            // this is necessary so that the command buffer processing doesn't get starved
            // with vsync enabled and a screen with a refresh rate of 60Hz or less
            if (display.vblank_count % 2 == 0)
                emuenv.renderer->should_display = true;
    }

    {
        const std::lock_guard<std::mutex> guard(display.mutex);
        callbacks_to_notify.reserve(display.vblank_callbacks.size());
        for (const auto &[_, cb] : display.vblank_callbacks)
            callbacks_to_notify.push_back(cb);
    }

    // maybe we should also use a mutex for this part, but it shouldn't be an issue
    touch_vsync_update(emuenv);
    refresh_motion(emuenv.motion, emuenv.ctrl);

    // Notify Vblank callback in each VBLANK start
    for (const auto &cb : callbacks_to_notify)
        cb->event_notify(emuenv.kernel, cb->get_notifier_id());

    std::vector<ThreadStatePtr> threads;
    {
        const std::lock_guard<std::mutex> kernel_lock(emuenv.kernel.mutex);
        threads.reserve(emuenv.kernel.threads.size());
        for (const auto &[_, thread] : emuenv.kernel.threads) {
            threads.push_back(thread);
        }
    }

    const uint64_t vblank_count = display.vblank_count.load(std::memory_order_relaxed);
    for (const auto &thread : threads) {
        const std::lock_guard<std::mutex> thread_lock(thread->mutex);
        if (!thread->has_wait_state() || thread->wait.type != WaitType::VBlank || thread->wait.target_value > vblank_count) {
            continue;
        }

        emuenv.kernel.thread_manager.wake_wait_locked(*thread);
    }
}

static void vblank_sync_thread(EmuEnvState &emuenv) {
    auto &display = emuenv.display;
    auto last_host_tick = std::chrono::steady_clock::now();

    while (!display.abort.load(std::memory_order_relaxed)) {
        const auto now = std::chrono::steady_clock::now();
        const auto elapsed_us = std::chrono::duration_cast<std::chrono::microseconds>(now - last_host_tick).count();
        last_host_tick = now;

        uint64_t accumulated_us = display.vblank_remainder_us.load(std::memory_order_relaxed);
        if (elapsed_us > 0)
            accumulated_us += static_cast<uint64_t>(elapsed_us);

        const uint64_t ticks_to_run = accumulated_us / TARGET_MICRO_PER_FRAME;
        accumulated_us %= TARGET_MICRO_PER_FRAME;
        display.vblank_remainder_us.store(accumulated_us, std::memory_order_relaxed);

        if (ticks_to_run == 0) {
            std::this_thread::sleep_for(std::chrono::microseconds(TARGET_MICRO_PER_FRAME - accumulated_us));
            continue;
        }

        for (uint64_t i = 0; i < ticks_to_run && !display.abort.load(std::memory_order_relaxed); ++i)
            run_vblank_tick(emuenv);
    }
}

void start_sync_thread(EmuEnvState &emuenv) {
    auto &display = emuenv.display;
    display.abort.store(true, std::memory_order_relaxed);
    if (display.vblank_thread && display.vblank_thread->joinable()) {
        display.vblank_thread->join();
    }

    display.abort.store(false, std::memory_order_relaxed);
    display.vblank_thread = std::make_unique<std::thread>(vblank_sync_thread, std::ref(emuenv));
}

void wait_vblank(DisplayState &display, KernelState &kernel, const ThreadStatePtr &wait_thread, const uint64_t target_vcount, const bool is_cb) {
    if (!wait_thread) {
        return;
    }

    bool run_callbacks = false;
    {
        auto thread_lock = std::unique_lock(wait_thread->mutex);
        if (target_vcount <= display.vblank_count)
            return;

        kernel.thread_manager.begin_wait_locked(*wait_thread, WaitState::vblank(target_vcount, is_cb));
        if (target_vcount <= display.vblank_count && wait_thread->wait.end_reason == WaitEndReason::None)
            kernel.thread_manager.wake_wait_locked(*wait_thread);

        wait_thread->state_cond.wait(thread_lock, [&]() {
            return wait_thread->wait.end_reason != WaitEndReason::None;
        });
        const WaitEndReason end_reason = wait_thread->wait.end_reason;
        kernel.thread_manager.clear_wait_locked(*wait_thread);
        run_callbacks = is_cb && end_reason == WaitEndReason::Signaled;
    }

    if (run_callbacks) {
        std::vector<CallbackPtr> callbacks_to_execute;
        {
            const std::lock_guard<std::mutex> guard(display.mutex);
            for (const auto &[_, cb] : display.vblank_callbacks) {
                if (cb->get_owner_thread_id() == wait_thread->id)
                    callbacks_to_execute.push_back(cb);
            }
        }

        for (const auto &cb : callbacks_to_execute) {
            cb->execute(kernel, []() {});
        }
    }
}

static void reset_swapchain_cycle(DisplayState &display, Address sync_object) {
    display.predicted_frames.resize(1);
    display.predicted_frames[0].sync_object = sync_object;
    display.predicted_frame_position = 0;
    display.predicted_cycles_seen = 0;
}

DisplayFrameInfo *predict_next_image(EmuEnvState &emuenv, Address sync_object) {
    auto &display = emuenv.display;
    std::lock_guard<std::mutex> lock(display.display_info_mutex);

    if (display.predicted_cycles_seen >= predict_threshold) {
        // just check that the next sync_object in line is the one we expect
        display.predicted_frame_position = (display.predicted_frame_position + 1) % display.predicted_frames.size();
        if (display.predicted_frames[display.predicted_frame_position].sync_object != sync_object)
            // bad, this isn't what we expect
            reset_swapchain_cycle(display, sync_object);

    } else if (display.predicted_cycles_seen >= 1) {
        display.predicted_frame_position = (display.predicted_frame_position + 1) % display.predicted_frames.size();

        if (display.predicted_frame_position == 0)
            display.predicted_cycles_seen++;

        if (display.predicted_frames[display.predicted_frame_position].sync_object != sync_object)
            // bad, this isn't what we expect
            reset_swapchain_cycle(display, sync_object);
    } else {
        // check if we have a cycle
        bool has_cycle = false;
        for (int idx = 0; idx < display.predicted_frames.size(); idx++) {
            if (display.predicted_frames[idx].sync_object == sync_object) {
                // we found a cycle
                has_cycle = true;
                display.predicted_frames.erase(display.predicted_frames.begin(), display.predicted_frames.begin() + idx);
                display.predicted_frame_position = 0;
                display.predicted_cycles_seen = 1;
                break;
            }
        }

        if (!has_cycle) {
            // predicted_frame_position is initialized to -1, so this is fine
            display.predicted_frame_position++;
            if (display.predicted_frame_position == display.predicted_frames.size()) {
                // keep the last max_expected_swapchain_size frames for the swapchain cycle
                if (display.predicted_frames.size() == max_expected_swapchain_size) {
                    display.predicted_frame_position = 0;
                } else {
                    display.predicted_frames.resize(display.predicted_frames.size() + 1);
                }
            }
            display.predicted_frames[display.predicted_frame_position].sync_object = sync_object;
        }
    }

    bool predict = display.predicted_cycles_seen >= predict_threshold;
    DisplayFrameInfo *frame = nullptr;
    if (predict) {
        // set the next framebuffer image here
        frame = new DisplayFrameInfo;
        *frame = display.predicted_frames[display.predicted_frame_position].frame_info;
    }

    return frame;
}

void update_prediction(EmuEnvState &emuenv, DisplayFrameInfo &frame) {
    auto &display = emuenv.display;
    std::lock_guard<std::mutex> lock(display.display_info_mutex);
    Address sync_object = display.current_sync_object;

    if (!display.predicting) {
        display.next_rendered_frame = frame;
        emuenv.renderer->should_display = true;
    }

    for (auto &pred_frame : display.predicted_frames) {
        if (pred_frame.sync_object != sync_object)
            continue;

        if (memcmp(&pred_frame.frame_info, &frame, sizeof(DisplayFrameInfo)) == 0)
            // we got what we expected, fine
            return;

        pred_frame.frame_info = frame;
        break;
    }

    if (display.predicting) {
        display.next_rendered_frame = frame;
        emuenv.renderer->should_display = true;
    }

    // let predict_next_image reset the cycle if necessary
    display.predicted_cycles_seen = std::min(display.predicted_cycles_seen, 1U);
}

void DisplayState::deinit() {
    abort.store(true, std::memory_order_relaxed);
    if (vblank_thread && vblank_thread->joinable()) {
        vblank_thread->join();
    }

    vblank_thread.reset();
    abort = false;

    {
        const std::lock_guard<std::mutex> guard(mutex);
        vblank_callbacks.clear();
    }

    {
        const std::lock_guard<std::mutex> guard(display_info_mutex);
        sce_frame = {};
        next_rendered_frame = {};
    }

    predicted_frames.clear();
    predicted_frame_position = static_cast<uint32_t>(-1);
    predicted_cycles_seen = 0;
    predicting = false;
    current_sync_object = 0;

    vblank_count = 0;
    vblank_remainder_us = 0;
    last_setframe_vblank_count = 0;

    fps_hack = false;
    // pretty sure we set this on game boot
    fullscreen = false;
}
