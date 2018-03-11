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

struct SDL_Window;

typedef std::shared_ptr<SDL_Window> WindowPtr;

struct HostState {
    std::string base_path;
    std::string pref_path;
    std::string game_title;
    std::string title_id;
    size_t frame_count = 0;
    uint32_t t1 = 0;
    WindowPtr window;
    MemState mem;
    CtrlState ctrl;
    KernelState kernel;
    AudioState audio;
    GxmState gxm;
    IOState io;
    NetState net;
};
