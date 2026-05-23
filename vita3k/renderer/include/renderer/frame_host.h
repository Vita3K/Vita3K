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

#include <cstdint>
#include <string>
#include <variant>
#include <vector>

typedef struct SDL_Window SDL_Window;

namespace renderer {
struct Win32DisplayHandle {
    void *hwnd = nullptr;
};

struct MacOSDisplayHandle {
    void *view = nullptr;
};

struct X11DisplayHandle {
    void *display = nullptr;
    std::uintptr_t window = 0;
    void *connection = nullptr;
};

struct WaylandDisplayHandle {
    void *display = nullptr;
    void *surface = nullptr;
};

struct AndroidDisplayHandle {
    SDL_Window *window = nullptr;
};

using DisplayHandle = std::variant<std::monostate,
    Win32DisplayHandle,
    MacOSDisplayHandle,
    X11DisplayHandle,
    WaylandDisplayHandle,
    AndroidDisplayHandle>;

class FrameHost {
public:
    virtual ~FrameHost() = default;

    virtual DisplayHandle handle() const = 0;
    virtual int drawable_width() const = 0;
    virtual int drawable_height() const = 0;
    virtual std::vector<std::string> font_dirs() const = 0;

    virtual void *get_proc_address(const char *name) const {
        return nullptr;
    }

    virtual unsigned int default_fbo() const {
        return 0;
    }

    virtual bool make_current() {
        return false;
    }

    virtual void done_current() {
    }

    virtual void swap_buffers() {
    }

    virtual bool set_vsync(bool enabled) {
        return false;
    }

    virtual void prepare_for_render_thread() {
    }

    virtual void finalize_render_thread_start() {
    }

    virtual void destroy_render_context() {
    }
};

} // namespace renderer
