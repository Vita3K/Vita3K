// Vita3K emulator project
// Copyright (C) 2026 Vita3K team
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "lightweight.h"
#include <util/log.h>

#include <array>
#include <new>

namespace threadmgr {

inline SceInt32 wait_process_thread_end(KernelState &kernel, SceUID waiter_id, SceUID target_id,
    Ptr<SceInt32> status, Ptr<SceUInt32> timeout, bool callbacks) {
    const auto waiter = kernel.get_thread(waiter_id);
    const auto target = kernel.get_thread(target_id);
    if (!waiter || !target || &waiter->cpu->mem != &target->cpu->mem)
        return SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID;
    if (waiter == target)
        return SCE_KERNEL_ERROR_ILLEGAL_THREAD_ID;

    MemState &mem = waiter->cpu->mem;
    try {
        std::array<GuestMemoryRange, 2> ranges{};
        size_t count = 0;
        if (status)
            ranges[count++] = { status.address(), sizeof(SceInt32) };
        if (timeout)
            ranges[count++] = { timeout.address(), sizeof(SceUInt32) };
        struct PinGuard {
            MemState &mem;
            uint64_t pin;
            ~PinGuard() {
                if (pin)
                    unpin_guest_memory(mem, pin);
            }
        };

        SceUInt32 time_left = 0;
        {
            const uint64_t pin = count ? pin_guest_memory(mem, std::span(ranges.data(), count)) : 0;
            if (count && !pin)
                return SCE_KERNEL_ERROR_ILLEGAL_ADDR;
            const PinGuard guard{ mem, pin };
            if (timeout && !copy_sync_guest_bytes(mem, timeout.address(), &time_left, sizeof(time_left), false))
                return SCE_KERNEL_ERROR_ILLEGAL_ADDR;
        }
        if (callbacks)
            waiter->process_callbacks();
        const Deadline deadline = deadline_from(timeout ? &time_left : nullptr);
        SceInt32 result_status = 0;
        const SceInt32 result = target->wait_for_thread_end(waiter, status ? &result_status : nullptr, deadline, callbacks);
        writeback_timeout(timeout ? &time_left : nullptr, deadline);
        const uint64_t pin = count ? pin_guest_memory(mem, std::span(ranges.data(), count)) : 0;
        if (count && !pin)
            return SCE_KERNEL_ERROR_ILLEGAL_ADDR;
        const PinGuard guard{ mem, pin };
        if (result == SCE_KERNEL_OK && status && !copy_sync_guest_bytes(mem, status.address(), &result_status, sizeof(result_status), true))
            return SCE_KERNEL_ERROR_ILLEGAL_ADDR;
        if (timeout && !copy_sync_guest_bytes(mem, timeout.address(), &time_left, sizeof(time_left), true))
            return SCE_KERNEL_ERROR_ILLEGAL_ADDR;
        return result;
    } catch (const std::bad_alloc &) {
        LOG_ERROR("Cannot pin guest arguments for thread-end wait");
        return SCE_KERNEL_ERROR_NO_MEMORY;
    }
}

} // namespace threadmgr