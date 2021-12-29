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

static void vblank_sync_thread(DisplayState &display) {
    while (!display.abort.load()) {
        {
            const std::lock_guard<std::mutex> guard(display.mutex);
            for (std::size_t i = 0; i < display.vblank_wait_infos.size(); i++) {
                if (--display.vblank_wait_infos[i].vsync_left == 0) {
                    ThreadStatePtr target_wait = display.vblank_wait_infos[i].target_thread;

                    target_wait->resume();
                    target_wait->status_cond.notify_all();

                    display.vblank_wait_infos.erase(display.vblank_wait_infos.begin() + i--);
                }
            }
            display.vblank_count++;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(TARGET_MS_PER_FRAME));
    }
}

void wait_vblank(DisplayState &display, KernelState &kernel, const SceUID thread_id, int count, const bool since_last_setbuf) {
    if (!display.vblank_thread) {
        display.vblank_thread = std::make_unique<std::thread>(vblank_sync_thread, std::ref(display));
    }

    const ThreadStatePtr wait_thread = util::find(thread_id, kernel.threads);
    if (!wait_thread) {
        return;
    }

    {
        const std::lock_guard<std::mutex> guard(display.mutex);
        if (since_last_setbuf) {
            const std::size_t blank_passed = display.vblank_count - display.last_setframe_vblank_count;
            if (blank_passed >= count) {
                return;
            } else {
                count -= static_cast<int>(blank_passed);
            }
        }
        display.vblank_wait_infos.push_back({ wait_thread, count });
    }

    auto thread_lock = std::unique_lock(wait_thread->mutex);
    thread_lock.unlock();

    wait_thread->suspend();
    wait_thread->status_cond.wait(thread_lock, [=]() { return wait_thread->status != ThreadStatus::suspend; });
}
