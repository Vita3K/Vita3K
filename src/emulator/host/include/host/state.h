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

#include "config.h"
#include <audio/state.h>
#include <ctrl/state.h>
#include <gui/state.h>
#include <gxm/state.h>
#include <host/sfo.h>
#include <io/state.h>
#include <kernel/state.h>
#include <net/state.h>
#include <nids/types.h>
#include <np/state.h>
#include <renderer/state.h>

#include <psp2/display.h>

#include <atomic>
#include <memory>
#include <string>

struct SDL_Window;
typedef void *SDL_GLContext;
typedef std::shared_ptr<SDL_Window> WindowPtr;
typedef std::unique_ptr<void, std::function<void(SDL_GLContext)>> GLContextPtr;

struct DisplayState {
    Ptr<const void> base;
    uint32_t pitch = 0;
    uint32_t pixelformat = SCE_DISPLAY_PIXELFORMAT_A8B8G8R8;
    SceIVector2 image_size = { 0, 0 };
    std::mutex mutex;
    std::condition_variable condvar;
    std::atomic<bool> abort{ false };
    std::atomic<bool> imgui_render{ true };
};

struct HostState {
    std::string game_title;
    std::string base_path;
    std::string pref_path;
    Config cfg;
    size_t frame_count = 0;
    uint32_t sdl_ticks = 0;
    WindowPtr window;
    GLContextPtr glcontext;
    SceIVector2 drawable_size = { 0, 0 };
    SceFVector2 viewport_pos = { 0, 0 };
    SceFVector2 viewport_size = { 0, 0 };
    MemState mem;
    CtrlState ctrl;
    KernelState kernel;
    AudioState audio;
    GxmState gxm;
    renderer::State renderer;
    IOState io;
    NetState net;
    NpState np;
    DisplayState display;
    GuiState gui;
    SfoFile sfo_handle;
    NIDSet missing_nids;
};
