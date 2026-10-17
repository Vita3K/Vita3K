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

#include <cpu/state.h>
#include <kernel/state.h>
#include <kernel/thread/thread_state.h>
#include <kernel/types.h>

#include <cstring>

namespace threadmgr {

// A size past the struct must be refused before calling this function.
template <typename T>
void write_info(T *info, T full, SceSize size) {
    full.size = sizeof(T);
    std::memcpy(info, &full, size);
}

template <typename T>
void write_info(T *info, T full) {
    write_info(info, full, info->size);
}

inline SceInt32 get_thread_info(KernelState &kernel, SceUID caller_id, SceUID target_id, Ptr<SceKernelThreadInfo> output) {
    const ThreadStatePtr target = kernel.get_thread(target_id ? target_id : caller_id);
    if (!target)
        return SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID;

    const ThreadStatePtr caller = kernel.get_thread(caller_id);
    if (!caller)
        return SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID;
    SceKernelThreadInfo *info = output.get(caller->cpu->mem);
    if (!info)
        return SCE_KERNEL_ERROR_ILLEGAL_ADDR;
    if (info->size > sizeof(*info))
        return SCE_KERNEL_ERROR_NO_MEMORY;

    // TODO: SCE_KERNEL_ERROR_ILLEGAL_CONTEXT check
    const auto full = target->info();
    if (!full)
        return SCE_KERNEL_ERROR_ERROR;
    write_info(info, *full);
    return SCE_KERNEL_OK;
}

} // namespace threadmgr