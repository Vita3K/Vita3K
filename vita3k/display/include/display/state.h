// Vita3K emulator project
// Copyright (C) 2025 Vita3K team
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

#include <kernel/callback.h>
#include <mem/ptr.h>
#include <util/types.h>

#include <atomic>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

enum SceDisplayPixelFormat {
    SCE_DISPLAY_PIXELFORMAT_A8B8G8R8 = 0x00000000U
};

struct ThreadState;
typedef std::shared_ptr<ThreadState> ThreadStatePtr;

struct DisplayStateVBlankWaitInfo {
    ThreadStatePtr target_thread;
    uint64_t target_vcount;
};

struct DisplayFrameInfo {
    Ptr<const void> base;
    uint32_t pitch = 0;
    uint32_t pixelformat = SCE_DISPLAY_PIXELFORMAT_A8B8G8R8;
    SceIVector2 image_size = { 0, 0 };
};

struct PredictedDisplayFrame {
    DisplayFrameInfo frame_info;
    Address sync_object;
};

struct DisplayState {
    // next frame as seen by SceDisplay
    DisplayFrameInfo sce_frame;

    std::mutex display_info_mutex;
    // the next frame which will actually be displayed by the renderer
    DisplayFrameInfo next_rendered_frame;

    std::mutex mutex;
    std::unique_ptr<std::thread> vblank_thread;
    std::atomic<bool> abort{ false };
    std::atomic<bool> imgui_render{ true };
    std::atomic<bool> fullscreen{ false };
    std::atomic<std::uint64_t> vblank_count{ 0 };
    std::vector<DisplayStateVBlankWaitInfo> vblank_wait_infos;
    std::atomic<uint64_t> last_setframe_vblank_count = 0;
    std::map<SceUID, CallbackPtr> vblank_callbacks{};

    // if set to true, make sceDisplayWaitVblankStartMulti/sceDisplayWaitSetFrameBufMulti behave as sceDisplayWaitVblankStart/sceDisplayWaitSetFrameBuf
    // this allows some game running at 30fps to run at 60fps without any issue
    // however this is not always the case, some games may not be affected (if they look at the Vcount)
    // or run twice as fast (if they only rely on these function calls for their timings)
    bool fps_hack = false;

    // should contain the list of sync objects / swapchain images (in the order they appear in the cycle)
    std::vector<PredictedDisplayFrame> predicted_frames;
    // position in the predicted_frame cycle (the -1 is needed)
    uint32_t predicted_frame_position = -1;
    // how many times the same predicted_frames cycle has been seen (if it is below some threshold, do not predict the frame)
    uint32_t predicted_cycles_seen = 0;
    // should the next call to sceDisplaySetFrameBuf do something
    std::atomic<bool> predicting = false;

    std::atomic<Address> current_sync_object;
};
