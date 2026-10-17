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

#include "../SceAppMgr/SceAppMgr.h"

#include <emuenv/app_util.h>
#include <mem/ptr.h>
#include <util/log.h>

#include <array>
#include <cstdint>

struct AppUtilInitParam {
    uint32_t work_buf_size;
    std::array<uint8_t, 60> reserved;
};

struct AppUtilBootParam {
    uint32_t attr;
    uint32_t app_version;
    std::array<uint8_t, 32> reserved;
};

static_assert(sizeof(AppUtilInitParam) == 64);
static_assert(sizeof(AppUtilBootParam) == 40);

template <typename InitializeSlots>
int initialize_secondary_app_util(MemState &mem, SecondaryAppUtilState &state, uint32_t sdk_version,
    Ptr<const void> init_param, Ptr<void> boot_param, InitializeSlots &&initialize_slots) {
    const std::lock_guard lock(state.mutex);
    if (sdk_version >= 0x01500000 && state.initialized)
        return static_cast<int>(SCE_APPUTIL_ERROR_BUSY);
    if (!init_param || !boot_param) {
        LOG_ERROR("sceAppUtilInit requires both parameter blocks");
        return static_cast<int>(SCE_APPUTIL_ERROR_PARAMETER);
    }

    const std::array ranges{
        GuestMemoryRange{ init_param.address(), sizeof(AppUtilInitParam) },
        GuestMemoryRange{ boot_param.address(), sizeof(AppUtilBootParam) },
    };
    const uint64_t pin = pin_guest_memory(mem, ranges);
    if (!pin) {
        LOG_ERROR("sceAppUtilInit received an invalid parameter range");
        return static_cast<int>(SCE_APPUTIL_ERROR_PARAMETER);
    }
    bool valid = true;
    for (uint32_t i = 0; i < sizeof(AppUtilInitParam); ++i)
        valid &= *Ptr<const uint8_t>(init_param.address() + i).get(mem) == 0;
    for (uint32_t i = 8; i < sizeof(AppUtilBootParam); ++i)
        valid &= *Ptr<const uint8_t>(boot_param.address() + i).get(mem) == 0;
    if (!valid) {
        unpin_guest_memory(mem, pin);
        LOG_ERROR("sceAppUtilInit received nonzero work buffer size or reserved fields");
        return static_cast<int>(SCE_APPUTIL_ERROR_PARAMETER);
    }

    for (uint32_t i = 0; i < 8; ++i)
        *Ptr<uint8_t>(boot_param.address() + i).get(mem) = 0;
    unpin_guest_memory(mem, pin);

    const int result = initialize_slots();
    if (result != 0 && result != static_cast<int>(0x80010013u))
        return result;
    state.initialized = true;
    return 0;
}

inline int shutdown_secondary_app_util(SecondaryAppUtilState &state) {
    const std::lock_guard lock(state.mutex);
    if (!state.initialized)
        return static_cast<int>(SCE_APPUTIL_ERROR_NOT_INITIALIZED);
    state.initialized = false;
    return 0;
}