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

#include <audio/state.h>
#include <ctrl/state.h>
#include <gxm/state.h>
#include <io/state.h>
#include <kernel/state.h>
#include <net/state.h>
#include <gui/state.h>
#include <psp2/display.h>

#include <atomic>

struct SDL_Window;
typedef void *SDL_GLContext;
typedef std::shared_ptr<SDL_Window> WindowPtr;
typedef std::unique_ptr<void, std::function<void(SDL_GLContext)>> GLContextPtr;

struct DisplayState {
    Ptr<const void> base;
    uint32_t pitch = 0;
    uint32_t pixelformat = SCE_DISPLAY_PIXELFORMAT_A8B8G8R8;
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t window_width = 0;
    uint32_t window_height = 0;
    std::mutex mutex;
    std::condition_variable condvar;
    std::atomic<bool> abort{ false };

    void set_window_dims(std::uint32_t width, std::uint32_t height)
    {
        window_width = width;
        window_height = height;
    }
};

struct HostState {
    std::string title_id;
    std::string game_title;
    std::string base_path;
    std::string pref_path;
    size_t frame_count = 0;
    uint32_t t1 = 0;
    WindowPtr window;
    GLContextPtr glcontext;
    MemState mem;
    CtrlState ctrl;
    KernelState kernel;
    AudioState audio;
    GxmState gxm;
    IOState io;
    NetState net;
    DisplayState display;
    GuiState gui;
};
