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

#include <host/sfo.h>
#include <host/window.h>

#include <audio/state.h>
#include <config/state.h>
#include <ctrl/state.h>
#include <dialog/state.h>
#include <gxm/state.h>
#include <io/state.h>
#include <kernel/state.h>
#include <net/state.h>
#include <nids/types.h>
#include <np/state.h>
#include <renderer/state.h>

// The GDB Stub requires winsock.h on windows (included in above headers). Keep it here to prevent build errors.
#ifdef USE_GDBSTUB
#include <gdbstub/state.h>
#endif

#include <atomic>
#include <memory>
#include <string>

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

struct HostState {
    std::string game_version;
    std::string game_title;
    std::string base_path;
    std::string default_path;
    std::string pref_path;
    ConfigState cfg;
    size_t frame_count = 0;
    uint32_t sdl_ticks = 0;
    uint32_t fps = 0;
    uint32_t ms_per_frame = 0;
    bool should_update_window_title = false;
    WindowPtr window;
    RendererPtr renderer;
    SceIVector2 drawable_size = { 0, 0 };
    SceFVector2 viewport_pos = { 0, 0 };
    SceFVector2 viewport_size = { 0, 0 };
    MemState mem;
    CtrlState ctrl;
    KernelState kernel;
    AudioState audio;
    GxmState gxm;
    bool renderer_focused;
    IOState io;
    NetState net;
    NpState np;
    DisplayState display;
    DialogState common_dialog;
    SfoFile sfo_handle;
    NIDSet missing_nids;
#ifdef USE_GDBSTUB
    GDBState gdb;
#endif
};
