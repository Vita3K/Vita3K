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

#include <kernel/callback.h>
#include <kernel/thread/wait_queue.h>
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

struct VBlankWaitEntry {
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
    // the renderer writes these under display_info_mutex, with the size of the frame they were laid out for
    int viewport_drawable_w = 0;
    int viewport_drawable_h = 0;
    float viewport_x = 0;
    float viewport_y = 0;
    float viewport_w = 0;
    float viewport_h = 0;
    SceIVector2 viewport_frame_size = { 0, 0 };

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
    // vblank_count in 60Hz units, this is what the game sees through sceDisplayGetVcount
    std::atomic<std::uint64_t> vcount_60hz{ 0 };
    // emulated vblank rate in Hz, 0 follows the refresh rate of the host screen
    std::atomic<int> vblank_rate{ 60 };
    // refresh rate of the screen the game window is on, only the Qt frontend updates it
    std::atomic<int> host_refresh_rate{ 60 };
    // the rate the vblank thread is running at right now, in Hz
    std::atomic<int> active_vblank_rate{ 60 };
    WaitQueue<VBlankWaitEntry> vblank_waiters;
    std::atomic<uint64_t> last_setframe_vblank_count = 0;
    std::map<SceUID, CallbackPtr> vblank_callbacks{};

    // should contain the list of sync objects / swapchain images (in the order they appear in the cycle)
    std::vector<PredictedDisplayFrame> predicted_frames;
    // position in the predicted_frame cycle (the -1 is needed)
    uint32_t predicted_frame_position = -1;
    // how many times the same predicted_frames cycle has been seen (if it is below some threshold, do not predict the frame)
    uint32_t predicted_cycles_seen = 0;
    // should the next call to sceDisplaySetFrameBuf do something
    std::atomic<bool> predicting = false;

    std::atomic<Address> current_sync_object;

    void deinit();
};
