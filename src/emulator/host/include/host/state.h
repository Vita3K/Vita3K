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
#include <gui/state.h>
#include <gxm/state.h>
#include <host/sfo.h>
#include <io/state.h>
#include <kernel/state.h>
#include <net/state.h>
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
    struct Size {
        uint32_t width = 0;
        uint32_t height = 0;

        Size() = default;
        Size(uint32_t w, uint32_t h)
            : width(w)
            , height(h) {}

        Size operator+(const Size &rhs) const {
            Size _size(this->width + rhs.width, this->height + rhs.height);
            return _size;
        }
    };

    Ptr<const void> base;
    uint32_t pitch = 0;
    uint32_t pixelformat = SCE_DISPLAY_PIXELFORMAT_A8B8G8R8;
    Size image_size; // Resolution of (guest) outputted video size
    Size border_size; // Window border size
    Size window_size; // Total window size (image_size + border_size)
    std::mutex mutex;
    std::condition_variable condvar;
    std::atomic<bool> abort{ false };
    std::atomic<bool> imgui_render{ true };

    void set_dims(std::uint32_t image_width, std::uint32_t image_height, std::uint32_t border_width, std::uint32_t border_height) {
        image_size = { image_width, image_height };
        border_size = { border_width, border_height };
        window_size = image_size + border_size;
    }
};

struct HostState {
    std::string game_title;
    std::string base_path;
    std::string pref_path;
    size_t frame_count = 0;
    uint32_t sdl_ticks = 0;
    WindowPtr window;
    GLContextPtr glcontext;
    MemState mem;
    CtrlState ctrl;
    KernelState kernel;
    AudioState audio;
    GxmState gxm;
    renderer::State renderer;
    IOState io;
    NetState net;
    DisplayState display;
    GuiState gui;
    SfoFile sfo_handle;
};
