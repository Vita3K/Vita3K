// Vita3K emulator project
// Copyright (C) 2023 Vita3K team
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

struct SDL_Thread;

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
    Ptr<SceGxmSyncObject> old_buffer;
    Ptr<SceGxmSyncObject> new_buffer;
    uint32_t new_buffer_timestamp;
};

struct MemoryMapInfo {
    Address offset;
    std::uint32_t size;
    std::uint32_t perm;
};

struct GxmState {
    SceGxmInitializeParams params;
    Queue<DisplayCallback> display_queue;
    Ptr<SceGxmSyncObject> last_fbo_sync_object;
    Ptr<uint32_t> notification_region;
    SceUID display_queue_thread;
    std::map<Address, MemoryMapInfo> memory_mapped_regions;
    std::mutex callback_lock;
    SDL_Thread *sdl_thread;
};
