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

#include <kernel/thread/thread_state.h>

#include <condition_variable>
#include <mutex>

struct KernelState;
class KernelThreadManager {
public:
    explicit KernelThreadManager(KernelState &kernel);

    bool is_paused() const;
    void pause_threads();
    void resume_threads();
    void wait_for_pause();

    void suspend(ThreadState &thread, SuspendType type);
    void suspend_locked(ThreadState &thread, SuspendType type);
    void resume(ThreadState &thread, SuspendType type, bool step = false);
    void resume_locked(ThreadState &thread, SuspendType type, bool step = false);
    void terminate(ThreadState &thread, bool run_end_callback);
    void terminate_locked(ThreadState &thread, bool run_end_callback);
    void begin_wait(ThreadState &thread, const WaitState &wait_state, WaitReason reason = WaitReason::Synchronization);
    void begin_wait_locked(ThreadState &thread, const WaitState &wait_state, WaitReason reason = WaitReason::Synchronization);
    void wake_wait(ThreadState &thread, WaitEndReason end_reason = WaitEndReason::Signaled);
    void wake_wait_locked(ThreadState &thread, WaitEndReason end_reason = WaitEndReason::Signaled);
    void timeout_wait(ThreadState &thread);
    void timeout_wait_locked(ThreadState &thread);
    void cancel_wait(ThreadState &thread, WaitEndReason end_reason = WaitEndReason::Canceled);
    void cancel_wait_locked(ThreadState &thread, WaitEndReason end_reason = WaitEndReason::Canceled);
    void cancel_wait(ThreadState &thread, uint64_t wait_sequence, WaitEndReason end_reason = WaitEndReason::Canceled);
    void cancel_wait_locked(ThreadState &thread, uint64_t wait_sequence, WaitEndReason end_reason = WaitEndReason::Canceled);
    void complete_wait(ThreadState &thread, uint64_t wait_sequence, const WaitState::WaitCompletion &completion, WaitEndReason end_reason = WaitEndReason::Signaled);
    void complete_wait_locked(ThreadState &thread, uint64_t wait_sequence, const WaitState::WaitCompletion &completion, WaitEndReason end_reason = WaitEndReason::Signaled);
    void clear_wait(ThreadState &thread);
    void clear_wait_locked(ThreadState &thread);
    void begin_guest_run(ThreadState &thread);
    void end_guest_run(ThreadState &thread);
    void notify_state_change();

private:
    bool is_paused_on_all_threads() const;

    KernelState &kernel;
    mutable std::mutex mutex;
    std::condition_variable state_change_cond;
    bool process_suspended = false;
};
