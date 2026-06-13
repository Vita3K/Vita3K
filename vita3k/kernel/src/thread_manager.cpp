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

#include <cpu/functions.h>
#include <kernel/thread_manager.h>

#include <kernel/state.h>

#include <vector>

namespace {

constexpr uint32_t suspend_flag(const SuspendType type) {
    return 1u << static_cast<uint32_t>(type);
}

WaitState *resolve_wait_state(ThreadState &thread, const uint64_t wait_sequence) {
    return thread.find_wait_state_by_sequence(wait_sequence);
}

const WaitState *resolve_wait_state(const ThreadState &thread, const uint64_t wait_sequence) {
    return thread.find_wait_state_by_sequence(wait_sequence);
}

} // namespace

KernelThreadManager::KernelThreadManager(KernelState &kernel)
    : kernel(kernel) {
}

bool KernelThreadManager::is_paused() const {
    const std::lock_guard<std::mutex> lock(mutex);
    return process_suspended;
}

void KernelThreadManager::pause_threads() {
    {
        const std::lock_guard<std::mutex> lock(mutex);
        process_suspended = true;
    }

    std::vector<ThreadStatePtr> threads;
    {
        const std::lock_guard<std::mutex> lock(kernel.mutex);
        threads.reserve(kernel.threads.size());
        for (const auto &[_, thread] : kernel.threads)
            threads.push_back(thread);
    }

    for (const auto &thread : threads)
        suspend(*thread, SuspendType::Process);

    wait_for_pause();
}

void KernelThreadManager::resume_threads() {
    {
        const std::lock_guard<std::mutex> lock(mutex);
        process_suspended = false;
    }

    std::vector<ThreadStatePtr> threads;
    {
        const std::lock_guard<std::mutex> lock(kernel.mutex);
        threads.reserve(kernel.threads.size());
        for (const auto &[_, thread] : kernel.threads)
            threads.push_back(thread);
    }

    for (const auto &thread : threads)
        resume(*thread, SuspendType::Process);

    notify_state_change();
}

void KernelThreadManager::wait_for_pause() {
    std::unique_lock<std::mutex> lock(mutex);
    state_change_cond.wait(lock, [this] {
        return !process_suspended || is_paused_on_all_threads();
    });
}

void KernelThreadManager::suspend(ThreadState &thread, const SuspendType type) {
    const std::lock_guard<std::mutex> thread_lock(thread.mutex);
    suspend_locked(thread, type);
}

void KernelThreadManager::suspend_locked(ThreadState &thread, const SuspendType type) {
    if (thread.is_suspend_requested(type))
        return;

    thread.suspend_flags |= suspend_flag(type);
    if (thread.state == RunState::Runnable)
        thread.wait_reason = WaitReason::Suspended;

    if (thread.is_executing()) {
        thread.suspend_requested = true;
        stop(*thread.cpu);
    } else {
        thread.state_cond.notify_all();
    }

    notify_state_change();
}

void KernelThreadManager::resume(ThreadState &thread, const SuspendType type, const bool step) {
    const std::lock_guard<std::mutex> thread_lock(thread.mutex);
    resume_locked(thread, type, step);
}

void KernelThreadManager::resume_locked(ThreadState &thread, const SuspendType type, const bool step) {
    thread.suspend_flags &= ~suspend_flag(type);
    thread.single_stepping = step;

    if (thread.suspend_flags == 0 && thread.state == RunState::Runnable)
        thread.wait_reason = WaitReason::None;

    thread.state_cond.notify_all();
    notify_state_change();
}

void KernelThreadManager::terminate(ThreadState &thread, const bool run_end_callback) {
    const std::lock_guard<std::mutex> thread_lock(thread.mutex);
    terminate_locked(thread, run_end_callback);
}

void KernelThreadManager::terminate_locked(ThreadState &thread, const bool run_end_callback) {
    thread.run_end_callback = run_end_callback;
    thread.delete_requested = true;

    if (thread.has_wait_state())
        cancel_wait_locked(thread, WaitEndReason::Deleted);

    if (thread.is_executing()) {
        stop(*thread.cpu);
    } else {
        thread.state_cond.notify_all();
    }

    notify_state_change();
}

void KernelThreadManager::begin_wait(ThreadState &thread, const WaitState &wait_state, const WaitReason reason) {
    const std::lock_guard<std::mutex> thread_lock(thread.mutex);
    begin_wait_locked(thread, wait_state, reason);
}

void KernelThreadManager::begin_wait_locked(ThreadState &thread, const WaitState &wait_state, const WaitReason reason) {
    thread.wait = wait_state;
    thread.wait_sequence = ++thread.wait_sequence_counter;
    thread.callback_wakeup_pending = false;
    thread.wait.end_reason = WaitEndReason::None;
    thread.wait.completion = {};
    thread.state = RunState::Waiting;
    thread.wait_reason = reason;

    thread.state_cond.notify_all();
    notify_state_change();
}

void KernelThreadManager::wake_wait(ThreadState &thread, const WaitEndReason end_reason) {
    const std::lock_guard<std::mutex> thread_lock(thread.mutex);
    wake_wait_locked(thread, end_reason);
}

void KernelThreadManager::wake_wait_locked(ThreadState &thread, const WaitEndReason end_reason) {
    if (!thread.has_wait_state())
        return;

    thread.wait.end_reason = end_reason;
    thread.wait.completion.end_reason = end_reason;
    thread.wait.completion.condition_met = (end_reason == WaitEndReason::Signaled);
    if (thread.state == RunState::Waiting)
        thread.state = RunState::Runnable;
    if (thread.suspend_flags == 0)
        thread.wait_reason = WaitReason::None;
    else if (thread.state == RunState::Runnable)
        thread.wait_reason = WaitReason::Suspended;

    thread.state_cond.notify_all();
    notify_state_change();
}

void KernelThreadManager::timeout_wait(ThreadState &thread) {
    const std::lock_guard<std::mutex> thread_lock(thread.mutex);
    timeout_wait_locked(thread);
}

void KernelThreadManager::timeout_wait_locked(ThreadState &thread) {
    wake_wait_locked(thread, WaitEndReason::Timeout);
}

void KernelThreadManager::cancel_wait(ThreadState &thread, const WaitEndReason end_reason) {
    const std::lock_guard<std::mutex> thread_lock(thread.mutex);
    cancel_wait_locked(thread, end_reason);
}

void KernelThreadManager::cancel_wait_locked(ThreadState &thread, const WaitEndReason end_reason) {
    wake_wait_locked(thread, end_reason);
}

void KernelThreadManager::cancel_wait(ThreadState &thread, const uint64_t wait_sequence, const WaitEndReason end_reason) {
    const std::lock_guard<std::mutex> thread_lock(thread.mutex);
    cancel_wait_locked(thread, wait_sequence, end_reason);
}

void KernelThreadManager::cancel_wait_locked(ThreadState &thread, const uint64_t wait_sequence, const WaitEndReason end_reason) {
    WaitState *wait_state = resolve_wait_state(thread, wait_sequence);
    if (!wait_state)
        return;

    if (thread.is_interrupted_wait_sequence(wait_sequence)) {
        wait_state->end_reason = end_reason;
        wait_state->completion.end_reason = end_reason;
        wait_state->completion.condition_met = (end_reason == WaitEndReason::Signaled);
        return;
    }

    wake_wait_locked(thread, end_reason);
}

void KernelThreadManager::complete_wait(ThreadState &thread, const uint64_t wait_sequence, const WaitState::WaitCompletion &completion, const WaitEndReason end_reason) {
    const std::lock_guard<std::mutex> thread_lock(thread.mutex);
    complete_wait_locked(thread, wait_sequence, completion, end_reason);
}

void KernelThreadManager::complete_wait_locked(ThreadState &thread, const uint64_t wait_sequence, const WaitState::WaitCompletion &completion, const WaitEndReason end_reason) {
    WaitState *wait_state = resolve_wait_state(thread, wait_sequence);
    if (!wait_state)
        return;

    wait_state->completion = completion;
    wait_state->completion.end_reason = end_reason;
    wait_state->end_reason = end_reason;

    if (thread.is_interrupted_wait_sequence(wait_sequence)) {
        wait_state->completion.condition_met = (end_reason == WaitEndReason::Signaled);
        return;
    }

    wake_wait_locked(thread, end_reason);
    thread.wait.completion = completion;
    thread.wait.completion.end_reason = end_reason;
}

void KernelThreadManager::clear_wait(ThreadState &thread) {
    const std::lock_guard<std::mutex> thread_lock(thread.mutex);
    clear_wait_locked(thread);
}

void KernelThreadManager::clear_wait_locked(ThreadState &thread) {
    thread.callback_wakeup_pending = false;
    thread.wait.clear();
    thread.wait_sequence = 0;
    if (thread.state != RunState::Waiting && thread.suspend_flags == 0)
        thread.wait_reason = WaitReason::None;
    notify_state_change();
}

void KernelThreadManager::begin_guest_run(ThreadState &thread) {
    thread.is_executing_guest = true;
    notify_state_change();
}

void KernelThreadManager::end_guest_run(ThreadState &thread) {
    thread.is_executing_guest = false;
    if (thread.suspend_flags != 0)
        thread.state_cond.notify_all();
    notify_state_change();
}

void KernelThreadManager::notify_state_change() {
    state_change_cond.notify_all();
}

bool KernelThreadManager::is_paused_on_all_threads() const {
    std::vector<ThreadStatePtr> threads;
    {
        const std::lock_guard<std::mutex> lock(kernel.mutex);
        threads.reserve(kernel.threads.size());
        for (const auto &[_, thread] : kernel.threads)
            threads.push_back(thread);
    }

    for (const auto &thread : threads) {
        const std::lock_guard<std::mutex> thread_lock(thread->mutex);
        if (thread->is_executing_guest)
            return false;
        if (thread->is_suspend_requested(SuspendType::Process)
            && thread->state == RunState::Runnable
            && thread->can_dispatch_guest()) {
            return false;
        }
    }

    return true;
}
