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

#include <kernel/core_timing.h>

#include <util/log.h>

namespace {

uint64_t host_now_us() {
    return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now().time_since_epoch())
            .count());
}

} // namespace

CoreTiming::~CoreTiming() {
    shutdown();
}

bool CoreTiming::init() {
    std::lock_guard<std::mutex> lock(mutex);
    if (initialized)
        return true;

    shutdown_requested = false;
    paused = false;
    guest_time_base_us = 0;
    host_resume_time_us = host_now_us();
    initialized = true;
    worker = std::thread(&CoreTiming::worker_loop, this);

    return true;
}

void CoreTiming::shutdown() {
    {
        std::lock_guard<std::mutex> lock(mutex);
        if (!initialized)
            return;

        shutdown_requested = true;
    }

    condvar.notify_all();
    if (worker.joinable())
        worker.join();

    std::lock_guard<std::mutex> lock(mutex);
    while (!queue.empty())
        queue.pop();
    live_events.clear();
    initialized = false;
    shutdown_requested = false;
    paused = false;
    guest_time_base_us = 0;
    next_handle = 1;
    next_fifo_order = 1;
}

void CoreTiming::pause(const bool should_pause) {
    std::lock_guard<std::mutex> lock(mutex);
    if (!initialized || paused == should_pause)
        return;

    const uint64_t current_host_now_us = host_now_us();
    if (should_pause) {
        guest_time_base_us = now_us_locked(current_host_now_us);
        paused = true;
    } else {
        host_resume_time_us = current_host_now_us;
        paused = false;
    }

    condvar.notify_all();
}

bool CoreTiming::is_paused() const {
    return paused.load(std::memory_order_acquire);
}

uint64_t CoreTiming::now_us() const {
    return now_us_locked(host_now_us());
}

CoreTimingHandle CoreTiming::schedule_at(uint64_t guest_deadline_us, std::string name, CoreTimingCallback callback) {
    {
        std::lock_guard<std::mutex> lock(mutex);
        if (!initialized)
            return 0;

        const auto handle = schedule_locked(guest_deadline_us, 0, false, std::move(name), std::move(callback));
        condvar.notify_all();
        return handle;
    }
}

CoreTimingHandle CoreTiming::schedule_at(uint64_t guest_deadline_us, CoreTimingCallback callback) {
    return schedule_at(guest_deadline_us, {}, std::move(callback));
}

CoreTimingHandle CoreTiming::schedule_after(uint64_t guest_delay_us, std::string name, CoreTimingCallback callback) {
    {
        std::lock_guard<std::mutex> lock(mutex);
        if (!initialized)
            return 0;

        const uint64_t deadline_us = now_us_locked(host_now_us()) + guest_delay_us;
        const auto handle = schedule_locked(deadline_us, 0, false, std::move(name), std::move(callback));
        condvar.notify_all();
        return handle;
    }
}

CoreTimingHandle CoreTiming::schedule_after(uint64_t guest_delay_us, CoreTimingCallback callback) {
    return schedule_after(guest_delay_us, {}, std::move(callback));
}

CoreTimingHandle CoreTiming::schedule_periodic(uint64_t first_deadline_us, uint64_t period_us, std::string name, CoreTimingCallback callback) {
    {
        std::lock_guard<std::mutex> lock(mutex);
        if (!initialized || period_us == 0)
            return 0;

        const auto handle = schedule_locked(first_deadline_us, period_us, true, std::move(name), std::move(callback));
        condvar.notify_all();
        return handle;
    }
}

CoreTimingHandle CoreTiming::schedule_periodic(uint64_t first_deadline_us, uint64_t period_us, CoreTimingCallback callback) {
    return schedule_periodic(first_deadline_us, period_us, {}, std::move(callback));
}

bool CoreTiming::cancel(const CoreTimingHandle handle) {
    std::lock_guard<std::mutex> lock(mutex);
    const auto it = live_events.find(handle);
    if (it == live_events.end())
        return false;

    live_events.erase(it);
    condvar.notify_all();
    return true;
}

uint64_t CoreTiming::now_us_locked(const uint64_t host_now_us) const {
    if (paused.load(std::memory_order_acquire))
        return guest_time_base_us.load(std::memory_order_acquire);

    const uint64_t base_us = guest_time_base_us.load(std::memory_order_acquire);
    const uint64_t resume_us = host_resume_time_us.load(std::memory_order_acquire);
    return base_us + (host_now_us - resume_us);
}

CoreTimingHandle CoreTiming::schedule_locked(const uint64_t guest_deadline_us, const uint64_t period_us, const bool repeating, std::string name, CoreTimingCallback callback) {
    EventState event;
    event.handle = next_handle++;
    event.deadline_us = guest_deadline_us;
    event.period_us = period_us;
    event.repeating = repeating;
    event.name = std::move(name);
    event.callback = std::move(callback);

    auto [it, inserted] = live_events.emplace(event.handle, std::move(event));
    auto &stored_event = it->second;
    queue.push({
        stored_event.handle,
        stored_event.generation,
        next_fifo_order++,
        stored_event.deadline_us,
    });

    return stored_event.handle;
}

bool CoreTiming::is_entry_current_locked(const QueueEntry &entry) const {
    const auto it = live_events.find(entry.handle);
    if (it == live_events.end())
        return false;

    const auto &event = it->second;
    return event.generation == entry.generation && event.deadline_us == entry.deadline_us;
}

void CoreTiming::discard_stale_entries_locked() {
    while (!queue.empty()) {
        if (!is_entry_current_locked(queue.top())) {
            queue.pop();
            continue;
        }
        break;
    }
}

void CoreTiming::worker_loop() {
    while (true) {
        struct DueEvent {
            CoreTimingHandle handle = 0;
            CoreTimingCallback callback;
            QueueEntry entry;
            uint64_t fire_count = 0;
        };

        std::vector<DueEvent> due_events;

        {
            std::unique_lock<std::mutex> lock(mutex);
            while (due_events.empty()) {
                discard_stale_entries_locked();

                if (shutdown_requested)
                    return;

                if (paused || queue.empty()) {
                    condvar.wait(lock);
                    continue;
                }

                const uint64_t current_host_now_us = host_now_us();
                const uint64_t guest_now_us = now_us_locked(current_host_now_us);
                const auto next_entry = queue.top();
                if (next_entry.deadline_us > guest_now_us) {
                    const auto wait_us = next_entry.deadline_us - guest_now_us;
                    condvar.wait_for(lock, std::chrono::microseconds(wait_us));
                    continue;
                }

                while (true) {
                    discard_stale_entries_locked();
                    if (queue.empty())
                        break;

                    const auto entry = queue.top();
                    const uint64_t current_guest_us = now_us_locked(host_now_us());
                    if (entry.deadline_us > current_guest_us)
                        break;

                    queue.pop();
                    const auto it = live_events.find(entry.handle);
                    if (it == live_events.end())
                        continue;

                    auto &event = it->second;
                    if (event.generation != entry.generation || event.deadline_us != entry.deadline_us)
                        continue;

                    const uint64_t next_fire_count = event.fire_count + 1;
                    CoreTimingCallback callback;

                    if (event.repeating && !shutdown_requested) {
                        callback = event.callback;
                        event.fire_count = next_fire_count;
                        event.deadline_us = entry.deadline_us + event.period_us;
                        event.generation++;
                        queue.push({
                            event.handle,
                            event.generation,
                            next_fifo_order++,
                            event.deadline_us,
                        });
                    } else {
                        callback = std::move(event.callback);
                        live_events.erase(it);
                    }

                    due_events.push_back({
                        entry.handle,
                        std::move(callback),
                        entry,
                        next_fire_count,
                    });
                }
            }
        }

        for (const auto &due_event : due_events) {
            const uint64_t fired_guest_us = now_us();
            const uint64_t lateness_us = fired_guest_us >= due_event.entry.deadline_us ? (fired_guest_us - due_event.entry.deadline_us) : 0;
            if (due_event.callback) {
                due_event.callback({
                    due_event.handle,
                    due_event.entry.deadline_us,
                    fired_guest_us,
                    lateness_us,
                    due_event.fire_count,
                });
            }
        }
    }
}
