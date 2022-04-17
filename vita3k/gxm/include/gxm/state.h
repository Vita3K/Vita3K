// Vita3K emulator project
// Copyright (C) 2021 Vita3K team
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

#include <gxm/types.h>
#include <mem/ptr.h>
#include <threads/queue.h>

#include <map>
#include <mutex>

typedef void SceGxmDisplayQueueCallback(Ptr<const void> callbackData);
static constexpr std::uint64_t SCENE_TIME_UNDEF = 0xFFFFFFFFFFFFFFFF;
static constexpr std::uint64_t GPU_SYNCING_DISABLE_SCENE_DELTA = 40;
static constexpr std::uint64_t GPU_INSERT_FENCE_SCENE_DELTA = 30;

struct SceGxmInitializeParams {
    uint32_t flags = 0;
    uint32_t displayQueueMaxPendingCount = 0;
    Ptr<SceGxmDisplayQueueCallback> displayQueueCallback;
    uint32_t displayQueueCallbackDataSize = 0;
    uint32_t parameterBufferSize = 0;
};

struct DisplayCallback {
    Address pc;
    Address data;
    Address old_buffer;
    Address new_buffer;
};

struct MemoryMapInfo {
    Address offset;
    std::uint32_t size;
    std::uint32_t perm;
};

struct SurfaceSyncingInfo {
    Address addr = 0;
    std::size_t size = 0;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::uint32_t stride = 0;
    SceGxmColorFormat sync_format;
    SceGxmColorSurfaceType surface_type;
    std::uint64_t scene_time_last_memaccess = SCENE_TIME_UNDEF;
    std::uint64_t scene_time_passed = 0;
    bool reprotect_needed = false;

    bool should_insert_protect_fence() const {
        return ((scene_time_last_memaccess == SCENE_TIME_UNDEF) || (scene_time_passed - scene_time_last_memaccess >= GPU_INSERT_FENCE_SCENE_DELTA));
    }

    bool syncing_disabled() const {
        return ((scene_time_last_memaccess == SCENE_TIME_UNDEF) || (scene_time_passed - scene_time_last_memaccess >= GPU_SYNCING_DISABLE_SCENE_DELTA));
    }
};

struct GxmState {
    SceGxmInitializeParams params;
    Queue<DisplayCallback> display_queue;
    Ptr<uint32_t> notification_region;
    SceUID display_queue_thread;
    std::map<Address, MemoryMapInfo> memory_mapped_regions;
    std::array<SurfaceSyncingInfo, 40> surface_syncing_infoes;
    std::mutex callback_lock;
};
