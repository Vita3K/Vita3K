// Vita3K emulator project
// Copyright (C) 2022 Vita3K team
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

#include <display/state.h>
#include <kernel/state.h>

#include <chrono>
#include <util/find.h>

// Code heavily influenced by PPSSSPP's SceDisplay.cpp

static constexpr int TARGET_FPS = 60;
static constexpr int TARGET_MS_PER_FRAME = 1000 / TARGET_FPS;

static void vblank_sync_thread(DisplayState &display, KernelState &kernel) {
    while (!display.abort.load()) {
        {
            const std::lock_guard<std::mutex> guard(display.mutex);
            // Notify Vblank callback in each VBLANK start
            for (auto &cb : display.vblank_callbacks)
                cb.second->event_notify(cb.second->get_notifier_id());

            for (std::size_t i = 0; i < display.vblank_wait_infos.size();) {
                auto &vblank_wait_info = display.vblank_wait_infos[i];
                if (--vblank_wait_info.vsync_left == 0) {
                    ThreadStatePtr target_wait = vblank_wait_info.target_thread;

                    target_wait->resume();
                    target_wait->status_cond.notify_all();

                    if (vblank_wait_info.is_cb) {
                        for (auto &callback : display.vblank_callbacks) {
                            CallbackPtr &cb = callback.second;
                            if (cb->get_owner_thread_id() == target_wait->id) {
                                std::string name = cb->get_name();
                                cb->execute(kernel, [name]() {
                                });
                            }
                        }
                    }

                    display.vblank_wait_infos.erase(display.vblank_wait_infos.begin() + i);
                } else {
                    i++;
                }
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(TARGET_MS_PER_FRAME));
        display.vblank_count++;
    }
}

void wait_vblank(DisplayState &display, KernelState &kernel, const ThreadStatePtr &wait_thread, int count, const bool since_last_setbuf, const bool is_cb) {
    if (!wait_thread) {
        return;
    }

    auto thread_lock = std::unique_lock(wait_thread->mutex);
    thread_lock.unlock();

    {
        const std::lock_guard<std::mutex> guard(display.mutex);

        if (!display.vblank_thread) {
            display.vblank_thread = std::make_unique<std::thread>(vblank_sync_thread, std::ref(display), std::ref(kernel));
        }

        if (since_last_setbuf) {
            const std::size_t blank_passed = display.vblank_count - display.last_setframe_vblank_count;
            if (blank_passed >= count) {
                return;
            } else {
                count -= static_cast<int>(blank_passed);
            }
        }

        wait_thread->suspend();
        display.vblank_wait_infos.push_back({ wait_thread, count, is_cb });
    }

    wait_thread->status_cond.wait(thread_lock, [=]() { return wait_thread->status != ThreadStatus::suspend; });
}
