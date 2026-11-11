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

#include <condition_variable>
#include <cstdint>
#include <list>
#include <mutex>

struct ThreadState;

class Scheduler {
public:
    static constexpr int CORE_COUNT = 4;
    static constexpr int NO_CORE = -1;
    static constexpr int SHARED_QUEUE_CPU = 0x7FFF;
    static constexpr int SHARED_AREA_PRIORITY = 0x80;

    enum class State : uint8_t {
        none,
        running,
        ready,
        standby,
    };

    struct Node {
        State state = State::none;
        int core = NO_CORE;
        int last_core = NO_CORE;
        int priority = 0;
        int32_t affinity_mask = 0;
        bool kicked = false;
        bool removed = false;
        std::condition_variable cv;
    };

    struct Info {
        State state;
        int current_cpu;
        int last_cpu;
    };

    void set_enabled(bool value) { enabled = value; }
    bool is_enabled() const { return enabled; }

    static ThreadState *current();
    static void set_current(ThreadState *thread);

    void make_ready(ThreadState &thread, ThreadState *from);
    void block(ThreadState &thread);
    void leave(ThreadState &thread);
    bool wait_for_core(ThreadState &thread);
    void kick(ThreadState &thread);
    void change_priority(ThreadState &thread, int priority);
    void change_affinity(ThreadState &thread, int32_t mask, ThreadState *from);
    Info info(ThreadState &thread);
    void reset();

private:
    struct Core {
        ThreadState *running = nullptr;
        std::list<ThreadState *> ready;
        std::list<ThreadState *> standby;
    };

    static unsigned cores_of(const ThreadState &thread);
    static bool in_shared_area(const ThreadState &thread);
    static void insert_by_priority(std::list<ThreadState *> &queue, ThreadState &thread, bool at_head);
    void remove_from_queues(ThreadState &thread);
    void run_on(int core, ThreadState &thread);
    void take_core(int core, ThreadState &thread);
    void give_up_core(ThreadState &thread);
    void place(ThreadState &thread, int from_core, bool preempted);
    void schedule_core(int core);
    ThreadState *best_waiting(int core, bool with_standby);
    int lowest_core(const ThreadState &thread, unsigned mask) const;
    void preempt_if_better(ThreadState &thread);

    bool enabled = true;
    std::mutex mutex;
    Core cores[CORE_COUNT];
    std::list<ThreadState *> shared_ready;
};