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
#include <kernel/state.h>
#include <kernel/thread/thread_state.h>
#include <kernel/thread/wait_functions.h>
#include <mem/ptr.h>

#include <limits>

namespace {

CoreTimingHandle arm_wait_timeout(KernelState &kernel, const ThreadStatePtr &thread, const WaitType expected_type, const uint64_t deadline_us, const SceUID expected_object = SCE_UID_INVALID_UID) {
    if (!thread || deadline_us == 0)
        return 0;

    const SceUID target_thread_id = thread->id;
    const uint64_t wait_sequence = thread->wait_sequence;

    return kernel.core_timing.schedule_at(deadline_us,
        [&kernel, target_thread_id, wait_sequence, expected_type, expected_object](const CoreTimingEventContext &) {
            const ThreadStatePtr target_thread = kernel.get_thread(target_thread_id);
            if (!target_thread)
                return;

            std::lock_guard<std::mutex> lock(target_thread->mutex);
            const WaitState *wait_state = target_thread->find_wait_state_by_sequence(wait_sequence);
            const bool same_wait = wait_state
                && wait_state->type == expected_type
                && (expected_object == SCE_UID_INVALID_UID || wait_state->object_id == expected_object);
            if (!same_wait)
                return;

            kernel.thread_manager.cancel_wait_locked(*target_thread, wait_sequence, WaitEndReason::Timeout);
        });
}

bool should_dispatch_wait_callbacks(const ThreadState &thread) {
    return thread.wait.callbacks_allowed
        && thread.callback_wakeup_pending
        && !thread.is_processing_callbacks.load(std::memory_order_acquire);
}

void wait_for_wait_end_or_callbacks(KernelState &kernel, std::unique_lock<std::mutex> &thread_lock, const ThreadStatePtr &thread) {
    while (thread->wait.end_reason == WaitEndReason::None) {
        if (should_dispatch_wait_callbacks(*thread)) {
            thread->callback_wakeup_pending = false;
            thread_lock.unlock();
            process_callbacks(kernel, thread->id);
            thread_lock.lock();
            continue;
        }

        thread->state_cond.wait(thread_lock, [&]() {
            return thread->wait.end_reason != WaitEndReason::None || should_dispatch_wait_callbacks(*thread);
        });
    }
}

void register_thread_end_waiter(ThreadState &target, const ThreadEndWaiterRef &waiter_ref) {
    target.thread_end_waiters.push_back(waiter_ref);
}

void remove_thread_end_waiter(ThreadState &target, const ThreadEndWaiterRef &waiter_ref) {
    std::erase_if(target.thread_end_waiters, [&](const ThreadEndWaiterRef &queued) {
        return queued.thread_id == waiter_ref.thread_id && queued.wait_sequence == waiter_ref.wait_sequence;
    });
}

} // namespace

uint64_t make_timeout_deadline_us(KernelState &kernel, const uint32_t timeout_us) {
    return kernel.core_timing.now_us() + timeout_us;
}

uint32_t remaining_timeout_us(const WaitState &wait, const uint64_t guest_now_us) {
    if (!wait.has_timeout || wait.timeout_deadline_us == 0 || wait.timeout_deadline_us <= guest_now_us)
        return 0;

    const uint64_t remaining = wait.timeout_deadline_us - guest_now_us;
    if (remaining > std::numeric_limits<uint32_t>::max())
        return std::numeric_limits<uint32_t>::max();

    return static_cast<uint32_t>(remaining);
}

uint64_t dispatch_pre_wait_callbacks(KernelState &kernel, const SceUID thread_id, const bool callbacks_allowed) {
    if (!callbacks_allowed)
        return 0;

    const uint64_t start = kernel.core_timing.now_us();
    process_callbacks(kernel, thread_id);
    return kernel.core_timing.now_us() - start;
}

bool wait_for_timeout_or_signal(KernelState &kernel, std::unique_lock<std::mutex> &thread_lock, const ThreadStatePtr &thread, const WaitType expected_type, const SceUID expected_object, uint32_t *timeout) {
    if (!timeout) {
        wait_for_wait_end_or_callbacks(kernel, thread_lock, thread);
        return true;
    }

    const uint32_t requested_timeout = *timeout;
    if (requested_timeout > 0 && thread->wait.timeout_deadline_us == 0)
        thread->wait.timeout_deadline_us = make_timeout_deadline_us(kernel, requested_timeout);

    const auto timeout_handle = requested_timeout > 0 ? arm_wait_timeout(kernel, thread, expected_type, thread->wait.timeout_deadline_us, expected_object) : 0;

    if (requested_timeout == 0)
        kernel.thread_manager.timeout_wait_locked(*thread);

    wait_for_wait_end_or_callbacks(kernel, thread_lock, thread);

    if (timeout_handle != 0)
        kernel.core_timing.cancel(timeout_handle);

    if (thread->wait.end_reason == WaitEndReason::Timeout) {
        *timeout = 0;
        return false;
    }

    *timeout = remaining_timeout_us(thread->wait, kernel.core_timing.now_us());
    return true;
}

SceInt32 wait_thread_signal(KernelState &kernel, const SceUID thread_id, const uint32_t delay, const uint32_t timeout, const bool callbacks_allowed) {
    // TODO: this isn't exactly as rich as on the actual vita (no payload/timeout-pointer)
    dispatch_pre_wait_callbacks(kernel, thread_id, callbacks_allowed);

    const auto thread = kernel.get_thread(thread_id);
    std::unique_lock<std::mutex> lock(thread->mutex);
    if (thread->signal_pending) {
        thread->signal_pending = false;
        return SCE_KERNEL_OK;
    }

    kernel.thread_manager.begin_wait_locked(*thread, WaitState::signal(delay, timeout, callbacks_allowed));
    uint32_t timeout_copy = timeout;
    if (!wait_for_timeout_or_signal(kernel, lock, thread, WaitType::Signal, SCE_UID_INVALID_UID, timeout > 0 ? &timeout_copy : nullptr)) {
        const auto end_reason = thread->wait.end_reason;
        kernel.thread_manager.clear_wait_locked(*thread);
        return end_reason == WaitEndReason::Timeout ? SCE_KERNEL_ERROR_WAIT_TIMEOUT : SCE_KERNEL_ERROR_WAIT_CANCEL;
    }

    const WaitEndReason end_reason = thread->wait.end_reason;
    if (end_reason == WaitEndReason::Signaled)
        thread->signal_pending = false;
    kernel.thread_manager.clear_wait_locked(*thread);

    switch (end_reason) {
    case WaitEndReason::Signaled:
        return SCE_KERNEL_OK;
    case WaitEndReason::Timeout:
        return SCE_KERNEL_ERROR_WAIT_TIMEOUT;
    case WaitEndReason::Deleted:
        return SCE_KERNEL_ERROR_WAIT_DELETE;
    default:
        return SCE_KERNEL_ERROR_WAIT_CANCEL;
    }
}

SceInt32 wait_thread_end(KernelState &kernel, MemState &mem, const ThreadStatePtr &waiter, const ThreadStatePtr &target, Ptr<int> stat, SceUInt *timeout, const bool callbacks_allowed) {
    dispatch_pre_wait_callbacks(kernel, waiter->id, callbacks_allowed);

    std::unique_lock<std::mutex> waiter_lock(waiter->mutex);
    ThreadEndWaiterRef waiter_ref{};
    {
        const std::unique_lock<std::mutex> thread_lock(target->mutex);
        if (target->is_dormant()) {
            if (stat)
                *stat.get(mem) = target->returned_value;
            return 0;
        }

        auto wait_state = WaitState::thread_end(target->id, callbacks_allowed);
        wait_state.requested_timeout = timeout ? *timeout : 0;
        wait_state.has_timeout = timeout != nullptr;
        if (timeout && *timeout > 0)
            wait_state.timeout_deadline_us = make_timeout_deadline_us(kernel, *timeout);
        wait_state.continuation.writeback.out_u32 = stat.address();
        kernel.thread_manager.begin_wait_locked(*waiter, wait_state);
        waiter_ref = ThreadEndWaiterRef{ waiter->id, waiter->wait_sequence };
        register_thread_end_waiter(*target, waiter_ref);
    }

    wait_for_timeout_or_signal(kernel, waiter_lock, waiter, WaitType::ThreadEnd, target->id, timeout);

    const auto wait_state = waiter->wait;
    kernel.thread_manager.clear_wait_locked(*waiter);
    waiter_lock.unlock();
    {
        const std::lock_guard<std::mutex> target_lock(target->mutex);
        remove_thread_end_waiter(*target, waiter_ref);
    }

    if (wait_state.end_reason == WaitEndReason::Signaled && wait_state.continuation.writeback.out_u32 != 0)
        *Ptr<uint32_t>(wait_state.continuation.writeback.out_u32).get(mem) = wait_state.completion.value_u32;

    switch (wait_state.end_reason) {
    case WaitEndReason::Signaled:
        if (timeout)
            *timeout = remaining_timeout_us(wait_state, kernel.core_timing.now_us());
        return SCE_KERNEL_OK;
    case WaitEndReason::Timeout:
        if (timeout)
            *timeout = 0;
        return SCE_KERNEL_ERROR_WAIT_TIMEOUT;
    case WaitEndReason::Deleted:
        if (timeout)
            *timeout = remaining_timeout_us(wait_state, kernel.core_timing.now_us());
        return SCE_KERNEL_ERROR_WAIT_DELETE;
    default:
        if (timeout)
            *timeout = remaining_timeout_us(wait_state, kernel.core_timing.now_us());
        return SCE_KERNEL_ERROR_WAIT_CANCEL;
    }
}

SceInt32 delay_thread(KernelState &kernel, const SceUID thread_id, SceUInt delay_us, const bool callbacks_allowed) {
    if (delay_us == 0)
        return SCE_KERNEL_ERROR_INVALID_ARGUMENT;

    const uint64_t callback_elapsed = dispatch_pre_wait_callbacks(kernel, thread_id, callbacks_allowed);
    if (callbacks_allowed) {
        if (callback_elapsed >= delay_us)
            return SCE_KERNEL_OK;
        delay_us -= static_cast<SceUInt>(callback_elapsed);
    }

    const ThreadStatePtr thread = kernel.get_thread(thread_id);
    std::unique_lock<std::mutex> lock(thread->mutex);

    auto wait_state = WaitState::delay(delay_us, callbacks_allowed);
    wait_state.timeout_deadline_us = make_timeout_deadline_us(kernel, delay_us);
    kernel.thread_manager.begin_wait_locked(*thread, wait_state);
    const auto delay_timeout = arm_wait_timeout(kernel, thread, WaitType::Delay, thread->wait.timeout_deadline_us);
    wait_for_wait_end_or_callbacks(kernel, lock, thread);
    if (delay_timeout != 0)
        kernel.core_timing.cancel(delay_timeout);
    kernel.thread_manager.clear_wait_locked(*thread);

    return SCE_KERNEL_OK;
}
