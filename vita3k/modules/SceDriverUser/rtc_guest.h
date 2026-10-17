// Vita3K emulator project
// Copyright (C) 2026 Vita3K team
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "../SceKernelThreadMgr/lightweight.h"
#include <rtc/rtc.h>

#include <array>
#include <new>

namespace rtc_guest {

template <typename Time>
SceInt32 get_time(MemState &mem, Ptr<const SceDateTime> date, Ptr<Time> output) {
    if (!date || !output)
        return static_cast<SceInt32>(SCE_RTC_ERROR_INVALID_POINTER);
    try {
        const std::array ranges{ GuestMemoryRange{ date.address(), sizeof(SceDateTime) },
            GuestMemoryRange{ output.address(), sizeof(Time) } };
        const uint64_t pin = pin_guest_memory(mem, ranges);
        if (!pin)
            return static_cast<SceInt32>(SCE_RTC_ERROR_INVALID_POINTER);
        struct PinGuard {
            MemState &mem;
            uint64_t pin;
            ~PinGuard() { unpin_guest_memory(mem, pin); }
        } guard{ mem, pin };
        SceDateTime value{};
        if (!threadmgr::copy_sync_guest_bytes(mem, date.address(), &value, sizeof(value), false))
            return static_cast<SceInt32>(SCE_RTC_ERROR_INVALID_POINTER);
        Time result = static_cast<Time>((__RtcPspTimeToTicks(&value) - RTC_OFFSET) / VITA_CLOCKS_PER_SEC);
        if (!threadmgr::copy_sync_guest_bytes(mem, output.address(), &result, sizeof(result), true))
            return static_cast<SceInt32>(SCE_RTC_ERROR_INVALID_POINTER);
        return 0;
    } catch (const std::bad_alloc &) {
        LOG_ERROR("Cannot pin guest arguments for RTC time conversion");
        return SCE_KERNEL_ERROR_NO_MEMORY;
    }
}

} // namespace rtc_guest