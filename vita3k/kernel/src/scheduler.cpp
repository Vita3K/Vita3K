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

#include <kernel/scheduler.h>

#include <cpu/state.h>
#include <kernel/thread/thread_state.h>

#include <algorithm>
#include <bit>

static thread_local ThreadState *current_guest_thread = nullptr;

ThreadState *Scheduler::current() {
    return current_guest_thread;
}

void Scheduler::set_current(ThreadState *thread) {
    current_guest_thread = thread;
}

unsigned Scheduler::cores_of(const ThreadState &thread) {
    constexpr unsigned all = (1u << CORE_COUNT) - 1;
    const unsigned mask = (static_cast<uint32_t>(thread.sched.affinity_mask) >> 16) & all;
    return mask ? mask : static_cast<unsigned>(SCE_KERNEL_CPU_MASK_USER_ALL >> 16);
}

bool Scheduler::in_shared_area(const ThreadState &thread) {
    return thread.sched.priority >= SHARED_AREA_PRIORITY;
}

void Scheduler::insert_by_priority(std::list<ThreadState *> &queue, ThreadState &thread, bool at_head) {
    const auto pos = std::find_if(queue.begin(), queue.end(), [&](const ThreadState *other) {
        return at_head ? other->sched.priority >= thread.sched.priority : other->sched.priority > thread.sched.priority;
    });
    queue.insert(pos, &thread);
}

void Scheduler::remove_from_queues(ThreadState &thread) {
    shared_ready.remove(&thread);
    for (Core &core : cores) {
        core.ready.remove(&thread);
        core.standby.remove(&thread);
    }
}

void Scheduler::run_on(int core, ThreadState &thread) {
    cores[core].running = &thread;
    thread.sched.state = State::running;
    thread.sched.core = core;
    thread.sched.last_core = core;
    thread.sched.cv.notify_all();
}

void Scheduler::take_core(int core, ThreadState &thread) {
    ThreadState *const victim = cores[core].running;
    run_on(core, thread);
    if (victim) {
        victim->sched.state = State::none;
        victim->sched.core = NO_CORE;
        victim->cpu->request_stop();
        place(*victim, NO_CORE, true);
    }
}

void Scheduler::give_up_core(ThreadState &thread) {
    const int core = thread.sched.core;
    cores[core].running = nullptr;
    thread.sched.state = State::none;
    thread.sched.core = NO_CORE;
    schedule_core(core);
}

int Scheduler::lowest_core(const ThreadState &thread, unsigned mask) const {
    int best = NO_CORE;
    for (int core = 0; core < CORE_COUNT; core++) {
        if (!(mask & (1u << core)) || !cores[core].running)
            continue;
        const int priority = cores[core].running->sched.priority;
        if (best == NO_CORE || priority > cores[best].running->sched.priority
            || (priority == cores[best].running->sched.priority && core == thread.sched.last_core))
            best = core;
    }
    return best;
}

void Scheduler::place(ThreadState &thread, int from_core, bool preempted) {
    const unsigned mask = cores_of(thread);
    int idle = NO_CORE;
    for (int core = 0; core < CORE_COUNT; core++) {
        if (!(mask & (1u << core)) || cores[core].running)
            continue;
        if (core == thread.sched.last_core) {
            idle = core;
            break;
        }
        if (idle == NO_CORE)
            idle = core;
    }
    if (idle != NO_CORE) {
        run_on(idle, thread);
        return;
    }
    const int target = lowest_core(thread, mask);
    if (thread.sched.priority < cores[target].running->sched.priority) {
        take_core(target, thread);
        return;
    }
    if (in_shared_area(thread)) {
        insert_by_priority(shared_ready, thread, preempted);
        thread.sched.state = State::ready;
        thread.sched.core = NO_CORE;
        return;
    }
    if (preempted) {
        const int last = thread.sched.last_core;
        const int core = (last != NO_CORE && (mask & (1u << last))) ? last : target;
        insert_by_priority(cores[core].ready, thread, true);
        thread.sched.state = State::ready;
        thread.sched.core = core;
        return;
    }
    if (from_core == NO_CORE || from_core == target) {
        insert_by_priority(cores[target].ready, thread, false);
        thread.sched.state = State::ready;
    } else {
        cores[target].standby.push_back(&thread);
        thread.sched.state = State::standby;
    }
    thread.sched.core = target;
}

ThreadState *Scheduler::best_waiting(int core, bool with_standby) {
    Core &c = cores[core];
    if (with_standby) {
        for (ThreadState *thread : c.standby) {
            insert_by_priority(c.ready, *thread, false);
            thread->sched.state = State::ready;
        }
        c.standby.clear();
    }
    ThreadState *local = c.ready.empty() ? nullptr : c.ready.front();
    const auto shared_it = std::find_if(shared_ready.begin(), shared_ready.end(), [&](const ThreadState *thread) {
        return cores_of(*thread) & (1u << core);
    });
    ThreadState *shared = shared_it == shared_ready.end() ? nullptr : *shared_it;
    if (!local)
        return shared;
    if (!shared || local->sched.priority <= shared->sched.priority)
        return local;
    return shared;
}

void Scheduler::schedule_core(int core) {
    ThreadState *const next = best_waiting(core, true);
    if (!next) {
        cores[core].running = nullptr;
        return;
    }
    remove_from_queues(*next);
    run_on(core, *next);
}

void Scheduler::preempt_if_better(ThreadState &thread) {
    if (thread.sched.core == NO_CORE) {
        const unsigned mask = cores_of(thread);
        for (int core = 0; core < CORE_COUNT; core++) {
            if ((mask & (1u << core)) && !cores[core].running) {
                remove_from_queues(thread);
                run_on(core, thread);
                return;
            }
        }
        const int target = lowest_core(thread, mask);
        if (target != NO_CORE && thread.sched.priority < cores[target].running->sched.priority) {
            remove_from_queues(thread);
            take_core(target, thread);
        }
        return;
    }
    const int core = thread.sched.core;
    ThreadState *const running = cores[core].running;
    if (!running) {
        remove_from_queues(thread);
        run_on(core, thread);
    } else if (thread.sched.priority < running->sched.priority) {
        remove_from_queues(thread);
        take_core(core, thread);
    }
}

void Scheduler::make_ready(ThreadState &thread, ThreadState *from) {
    if (!enabled)
        return;
    const std::lock_guard<std::mutex> lock(mutex);
    if (thread.sched.removed || thread.sched.state != State::none)
        return;
    const int from_core = (from && from->sched.state == State::running) ? from->sched.core : NO_CORE;
    place(thread, from_core, false);
}

void Scheduler::block(ThreadState &thread) {
    if (!enabled)
        return;
    const std::lock_guard<std::mutex> lock(mutex);
    switch (thread.sched.state) {
    case State::running:
        give_up_core(thread);
        break;
    case State::ready:
    case State::standby:
        remove_from_queues(thread);
        thread.sched.state = State::none;
        thread.sched.core = NO_CORE;
        break;
    case State::none:
        break;
    }
}

void Scheduler::leave(ThreadState &thread) {
    if (!enabled)
        return;
    const std::lock_guard<std::mutex> lock(mutex);
    switch (thread.sched.state) {
    case State::running:
        give_up_core(thread);
        break;
    case State::ready:
    case State::standby:
        remove_from_queues(thread);
        thread.sched.state = State::none;
        thread.sched.core = NO_CORE;
        break;
    case State::none:
        break;
    }
    thread.sched.removed = true;
    thread.sched.cv.notify_all();
}

bool Scheduler::wait_for_core(ThreadState &thread) {
    if (!enabled)
        return true;
    std::unique_lock<std::mutex> lock(mutex);
    if (thread.sched.state == State::none && !thread.sched.removed)
        place(thread, NO_CORE, false);
    thread.sched.cv.wait(lock, [&] { return thread.sched.state == State::running || thread.sched.removed || thread.sched.kicked; });
    thread.sched.kicked = false;
    return thread.sched.state == State::running;
}

void Scheduler::kick(ThreadState &thread) {
    if (!enabled)
        return;
    const std::lock_guard<std::mutex> lock(mutex);
    thread.sched.kicked = true;
    thread.sched.cv.notify_all();
}

void Scheduler::change_priority(ThreadState &thread, int priority) {
    const std::lock_guard<std::mutex> lock(mutex);
    const int old = thread.sched.priority;
    thread.sched.priority = priority;
    if (!enabled)
        return;
    switch (thread.sched.state) {
    case State::running: {
        if (priority <= old)
            break;
        ThreadState *const best = best_waiting(thread.sched.core, true);
        if (best && best->sched.priority < priority) {
            const int core = thread.sched.core;
            thread.cpu->request_stop();
            give_up_core(thread);
            place(thread, core, false);
        }
        break;
    }
    case State::ready: {
        remove_from_queues(thread);
        if (in_shared_area(thread)) {
            insert_by_priority(shared_ready, thread, false);
            thread.sched.core = NO_CORE;
        } else {
            int core = thread.sched.core;
            if (core == NO_CORE) {
                const unsigned mask = cores_of(thread);
                core = (thread.sched.last_core != NO_CORE && (mask & (1u << thread.sched.last_core))) ? thread.sched.last_core : std::countr_zero(mask);
            }
            insert_by_priority(cores[core].ready, thread, false);
            thread.sched.core = core;
        }
        preempt_if_better(thread);
        break;
    }
    case State::standby:
        if (in_shared_area(thread)) {
            remove_from_queues(thread);
            insert_by_priority(shared_ready, thread, false);
            thread.sched.state = State::ready;
            thread.sched.core = NO_CORE;
        }
        preempt_if_better(thread);
        break;
    case State::none:
        break;
    }
}

void Scheduler::change_affinity(ThreadState &thread, int32_t mask, ThreadState *from) {
    const std::lock_guard<std::mutex> lock(mutex);
    thread.sched.affinity_mask = mask;
    if (!enabled)
        return;
    const unsigned cores_mask = cores_of(thread);
    const int from_core = (from && from->sched.state == State::running) ? from->sched.core : NO_CORE;
    switch (thread.sched.state) {
    case State::running:
        if (!(cores_mask & (1u << thread.sched.core))) {
            thread.cpu->request_stop();
            give_up_core(thread);
            place(thread, from_core, false);
        }
        break;
    case State::ready:
    case State::standby:
        if (thread.sched.core == NO_CORE) {
            preempt_if_better(thread);
        } else if (!(cores_mask & (1u << thread.sched.core))) {
            remove_from_queues(thread);
            thread.sched.state = State::none;
            thread.sched.core = NO_CORE;
            place(thread, from_core, false);
        }
        break;
    case State::none:
        break;
    }
}

Scheduler::Info Scheduler::info(ThreadState &thread) {
    const std::lock_guard<std::mutex> lock(mutex);
    Info result{ thread.sched.state, NO_CORE, thread.sched.last_core };
    switch (thread.sched.state) {
    case State::running:
    case State::standby:
        result.current_cpu = thread.sched.core;
        break;
    case State::ready:
        result.current_cpu = thread.sched.core == NO_CORE ? SHARED_QUEUE_CPU : thread.sched.core;
        break;
    case State::none:
        break;
    }
    return result;
}

void Scheduler::reset() {
    const std::lock_guard<std::mutex> lock(mutex);
    for (Core &core : cores) {
        core.running = nullptr;
        core.ready.clear();
        core.standby.clear();
    }
    shared_ready.clear();
}