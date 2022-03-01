// Vita3K emulator project
// Copyright (C) 2022 Vita3K team
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

#include <atomic>
#include <mem/ptr.h>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include <util/types.h>

enum SceDisplayPixelFormat {
    SCE_DISPLAY_PIXELFORMAT_A8B8G8R8 = 0x00000000U
};

struct ThreadState;
typedef std::shared_ptr<ThreadState> ThreadStatePtr;

struct DisplayStateVBlankWaitInfo {
    ThreadStatePtr target_thread;
    std::int32_t vsync_left;
};

struct DisplayState {
    Ptr<const void> base;
    uint32_t pitch = 0;
    uint32_t pixelformat = SCE_DISPLAY_PIXELFORMAT_A8B8G8R8;
    SceIVector2 image_size = { 0, 0 };
    std::mutex mutex;
    std::mutex display_info_mutex;
    std::unique_ptr<std::thread> vblank_thread;
    std::atomic<bool> abort{ false };
    std::atomic<bool> imgui_render{ true };
    std::atomic<bool> fullscreen{ false };
    std::atomic<std::uint64_t> vblank_count{ 0 };
    std::vector<DisplayStateVBlankWaitInfo> vblank_wait_infos;
    std::uint64_t last_setframe_vblank_count = 0;
};
