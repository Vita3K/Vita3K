// Vita3K emulator project
// Copyright (C) 2026 Vita3K team
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <kernel/state.h>
#include <kernel/thread/thread_state.h>

#include <algorithm>
#include <chrono>

namespace threadmgr {

inline SceInt32 delay_thread(KernelState &kernel, SceUID thread_id, SceUInt delay_us, bool callbacks, bool minimum_201) {
    if (minimum_201)
        delay_us = std::max(delay_us, 201u);
    if (!delay_us)
        return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
    const auto thread = kernel.get_thread(thread_id);
    if (!thread)
        return SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID;
    delay_us = std::max(delay_us, 200u);
    const Deadline deadline = std::chrono::steady_clock::now() + std::chrono::microseconds(delay_us);
    return thread->delay_until(deadline, callbacks);
}

} // namespace threadmgr