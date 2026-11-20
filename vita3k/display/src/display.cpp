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

#include <cheat/functions.h>
#include <ctrl/functions.h>
#include <dialog/state.h>
#include <display/state.h>
#include <emuenv/state.h>
#include <kernel/state.h>
#include <renderer/state.h>

#include <algorithm>
#include <chrono>
#include <motion/functions.h>
#include <touch/functions.h>
#include <util/log.h>

// Code heavily influenced by PPSSSPP's SceDisplay.cpp

static constexpr int NATIVE_VBLANK_RATE = 60;
// never below the native rate, the touch panel and the motion sensor are sampled on vblanks and must keep their 60 Hz
static constexpr int MIN_VBLANK_RATE = NATIVE_VBLANK_RATE;
static constexpr int MAX_VBLANK_RATE = 240;
// how many cycles do we need to see before we start predicting the next frame
static constexpr int predict_threshold = 3;
static constexpr int max_expected_swapchain_size = 6;
// The bit of the first display head in its vblank event
static constexpr SceUInt32 VBLANK_EVENT_BIT = 0x1;

static int current_vblank_rate(const DisplayState &display) {
    const int rate = display.vblank_rate.load();
    return std::clamp(rate > 0 ? rate : display.host_refresh_rate.load(), MIN_VBLANK_RATE, MAX_VBLANK_RATE);
}

static void vblank_sync_thread(EmuEnvState &emuenv) {
    DisplayState &display = emuenv.display;
    // what the game sees through sceDisplayGetVcount, it advances with the vblanks to stay in step with the wait functions
    double vcount_60hz = 0;
    double paused_redraw_credit = 0;
    int logged_rate = NATIVE_VBLANK_RATE;

    const cheat::JitInvalidate invalidate_jit = [&emuenv](uint32_t address, size_t size) {
        emuenv.kernel.invalidate_jit_cache(address, size);
    };

    while (!display.abort.load()) {
        const int rate = current_vblank_rate(display);
        display.active_vblank_rate = rate;
        // the rate can change while the app runs (settings, or the window moving to another screen)
        if (rate != logged_rate) {
            if (rate != NATIVE_VBLANK_RATE)
                LOG_WARN("Emulated vblank rate: {} Hz (setting: {}, host: {} Hz)", rate, display.vblank_rate.load(), display.host_refresh_rate.load());
            logged_rate = rate;
        }
        // the touch panel, the motion sensor and the cheats keep their 60 Hz whatever the vblank rate,
        // so the history they keep, their filters and their delays behave as they do on the console
        vcount_60hz += static_cast<double>(NATIVE_VBLANK_RATE) / rate;
        const uint64_t next_vcount = static_cast<uint64_t>(vcount_60hz);
        const bool native_vblank = next_vcount != display.vcount_60hz;
        {
            // Before the count goes up, so a read that waited for this vblank gets its samples
            if (native_vblank) {
                touch_vsync_update(emuenv);
                refresh_motion(emuenv.motion, emuenv.ctrl);
            }

            bool has_frame = false;
            {
                const std::lock_guard<std::mutex> guard_info(display.display_info_mutex);
                ++display.vblank_count;
                display.vcount_60hz = next_vcount;

                has_frame = static_cast<bool>(display.sce_frame.base);

                // in this case, even though no new game frames are being rendered, we still need to update the screen
                if (emuenv.kernel.is_threads_paused() || (emuenv.common_dialog.status == SCE_COMMON_DIALOG_STATUS_RUNNING)) {
                    // only display the UI/common dialog at 30 fps
                    // this is necessary so that the command buffer processing doesn't get starved
                    // with vsync enabled and a screen with a refresh rate of 60Hz or less
                    // the credit grows by 30 every second whatever the vblank rate, so the average stays at 30 fps
                    paused_redraw_credit += 30.0 / rate;
                    if (paused_redraw_credit >= 1.0) {
                        paused_redraw_credit -= 1.0;
                        emuenv.renderer->should_display = true;
                    }
                }
            }

            if (native_vblank) {
                if (has_frame)
                    emuenv.cheat.count_frame();

                // Cheat codes are re-applied on every vblank, the same way the Vita cheat plugins do.
                // The pad is read here rather than taken from the game, which may stop reading it while a button is held.
                cheat::set_buttons(emuenv.cheat, ctrl_peek_buttons(emuenv, 1));
                cheat::apply(emuenv.cheat, emuenv.mem, invalidate_jit);
            }

            // Wakes the display waits and notifies the vblank start callbacks
            // they follow the emulated vblank rate, this is what lets an app run above 60 FPS
            if (const SimpleEventRef event = emuenv.kernel.kernel_objects.find<SimpleEvent>(display.vblank_event))
                event->pulse_and_notify(VBLANK_EVENT_BIT, 0);
        }
        const auto time_ms = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        const int64_t micro_per_frame = 1000000LL / rate;
        const auto time_left = micro_per_frame - (time_ms % micro_per_frame);
        std::this_thread::sleep_for(std::chrono::microseconds(time_left));
    }
}

void start_sync_thread(EmuEnvState &emuenv) {
    // The kernel owns the event, in the table no guest UID reaches
    emuenv.display.vblank_event = emuenv.kernel.kernel_objects.add(SimpleEventRef::make(0, "SceDisplayVblank", 0, 0), emuenv.kernel.get_next_uid());
    emuenv.display.vblank_thread = std::make_unique<std::thread>(vblank_sync_thread, std::ref(emuenv));
}

SceInt32 wait_vblank_start(EmuEnvState &emuenv, ThreadState &thread, int count, bool callbacks) {
    // The reference keeps the event alive while the thread waits on it
    const SimpleEventRef event = emuenv.kernel.kernel_objects.find<SimpleEvent>(emuenv.display.vblank_event);
    if (!event)
        return SCE_KERNEL_ERROR_UNKNOWN_EVENT_ID;
    // A wait in a callback goes one level deeper and starts from the base of the wait around it
    uint32_t &depth = thread.display_waits.depth;
    auto &bases = thread.display_waits.bases;
    const uint64_t base = bases[std::min<size_t>(depth, std::size(bases) - 1)];
    ++depth;
    if (depth < std::size(bases))
        bases[depth] = bases[depth - 1];
    SceInt32 result;
    uint64_t now;
    do {
        result = event->wait(thread, VBLANK_EVENT_BIT, nullptr, nullptr, nullptr, callbacks);
        now = emuenv.display.vblank_count;
    } while (result >= 0 && static_cast<int64_t>(now - base) < count);
    --depth;
    if (depth < std::size(bases))
        bases[depth] = now;
    return result;
}

SceInt32 wait_until_vblank(EmuEnvState &emuenv, ThreadState &thread, uint64_t target, bool callbacks) {
    const SimpleEventRef event = emuenv.kernel.kernel_objects.find<SimpleEvent>(emuenv.display.vblank_event);
    if (!event)
        return SCE_KERNEL_ERROR_UNKNOWN_EVENT_ID;
    while (emuenv.display.vblank_count < target) {
        if (const SceInt32 result = event->wait(thread, VBLANK_EVENT_BIT, nullptr, nullptr, nullptr, callbacks); result < 0)
            return result;
    }
    return SCE_KERNEL_OK;
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
        LOG_TRACE("Mispredicted the next swapchain image");
        display.next_rendered_frame = frame;
        emuenv.renderer->should_display = true;
    }

    // let predict_next_image reset the cycle if necessary
    display.predicted_cycles_seen = std::min(display.predicted_cycles_seen, 1U);
}

void DisplayState::deinit(KernelState &kernel) {
    abort = true;
    if (vblank_thread && vblank_thread->joinable())
        vblank_thread->join();

    vblank_thread.reset();
    abort = false;
    // The vblank thread has stopped. Removing the event ends any wait left on it and drops the
    // registered callbacks.
    kernel.kernel_objects.remove<SimpleEvent>(vblank_event, 0);
    vblank_event = 0;

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
    vcount_60hz = 0;
    last_setframe_vblank_count = 0;

    vblank_rate = 60;
    // pretty sure we set this on game boot
    fullscreen = false;
}