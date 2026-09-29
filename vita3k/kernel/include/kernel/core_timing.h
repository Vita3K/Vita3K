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

#include <functional>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include <atomic>
#include <condition_variable>
#include <cstdint>

using CoreTimingHandle = uint64_t;

struct CoreTimingEventContext {
    CoreTimingHandle handle = 0;
    uint64_t scheduled_deadline_us = 0;
    uint64_t fired_guest_time_us = 0;
    uint64_t lateness_us = 0;
    uint64_t fire_count = 0;
};

using CoreTimingCallback = std::function<void(const CoreTimingEventContext &)>;

class CoreTiming {
public:
    CoreTiming() = default;
    ~CoreTiming();

    bool init();
    void shutdown();

    void pause(bool should_pause);
    bool is_paused() const;

    uint64_t now_us() const;

    CoreTimingHandle schedule_at(uint64_t guest_deadline_us, std::string name, CoreTimingCallback callback);
    CoreTimingHandle schedule_at(uint64_t guest_deadline_us, CoreTimingCallback callback);
    CoreTimingHandle schedule_after(uint64_t guest_delay_us, std::string name, CoreTimingCallback callback);
    CoreTimingHandle schedule_after(uint64_t guest_delay_us, CoreTimingCallback callback);
    CoreTimingHandle schedule_periodic(uint64_t first_deadline_us, uint64_t period_us, std::string name, CoreTimingCallback callback);
    CoreTimingHandle schedule_periodic(uint64_t first_deadline_us, uint64_t period_us, CoreTimingCallback callback);
    bool cancel(CoreTimingHandle handle);

private:
    struct EventState {
        CoreTimingHandle handle = 0;
        uint64_t deadline_us = 0;
        uint64_t period_us = 0;
        uint64_t fire_count = 0;
        uint64_t generation = 1;
        bool repeating = false;
        std::string name;
        CoreTimingCallback callback;
    };

    struct QueueEntry {
        CoreTimingHandle handle = 0;
        uint64_t generation = 0;
        uint64_t fifo_order = 0;
        uint64_t deadline_us = 0;
    };

    struct EventCompare {
        bool operator()(const QueueEntry &lhs, const QueueEntry &rhs) const {
            if (lhs.deadline_us != rhs.deadline_us)
                return lhs.deadline_us > rhs.deadline_us;
            return lhs.fifo_order > rhs.fifo_order;
        }
    };

    uint64_t now_us_locked(uint64_t host_now_us) const;
    CoreTimingHandle schedule_locked(uint64_t guest_deadline_us, uint64_t period_us, bool repeating, std::string name, CoreTimingCallback callback);
    bool is_entry_current_locked(const QueueEntry &entry) const;
    void discard_stale_entries_locked();
    void worker_loop();

    mutable std::mutex mutex;
    std::condition_variable condvar;
    std::priority_queue<QueueEntry, std::vector<QueueEntry>, EventCompare> queue;
    std::unordered_map<CoreTimingHandle, EventState> live_events;
    std::thread worker;

    std::atomic<uint64_t> host_resume_time_us{ 0 };
    std::atomic<uint64_t> guest_time_base_us{ 0 };
    CoreTimingHandle next_handle = 1;
    uint64_t next_fifo_order = 1;
    std::atomic<bool> initialized{ false };
    std::atomic<bool> shutdown_requested{ false };
    std::atomic<bool> paused{ false };
};
