// Vita3K emulator project
// Copyright (C) 2018 Vita3K team
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

#include <kernel/types.h>
#include <mem/ptr.h>

#include <atomic>
#include <condition_variable>
#include <mutex>

struct DisplayState {
    Ptr<const void> base;
    uint32_t pitch = 0;
    uint32_t pixelformat = SCE_DISPLAY_PIXELFORMAT_A8B8G8R8;
    SceIVector2 image_size = { 0, 0 };
    std::mutex mutex;
    std::condition_variable condvar;
    std::atomic<bool> abort{ false };
    std::atomic<bool> imgui_render{ true };
    std::atomic<bool> fullscreen{ false };
};
