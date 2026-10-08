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

// Same as vkutil, or the include guard keeps the beta types out of later Vulkan includes
#ifdef __APPLE__
#define VK_ENABLE_BETA_EXTENSIONS
#endif
#include <vulkan/vulkan_core.h>

#include <cstdint>
#include <string>
#include <variant>
#include <vector>

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

using DisplayHandle = std::variant<std::monostate,
    Win32DisplayHandle,
    MacOSDisplayHandle,
    X11DisplayHandle,
    WaylandDisplayHandle>;

// Makes Vulkan surfaces for a window whose toolkit knows how, like SDL
class VulkanSurfaceProvider {
public:
    virtual ~VulkanSurfaceProvider() = default;

    // The instance extensions a surface needs. The strings stay valid while the provider exists.
    virtual std::vector<const char *> instance_extensions() const = 0;
    // Returns a new surface, or VK_NULL_HANDLE if there is no window to make it for yet
    virtual VkSurfaceKHR create_surface(VkInstance instance) const = 0;
    // Destroys a surface made by create_surface, through the loader that made it
    virtual void destroy_surface(VkInstance instance, VkSurfaceKHR surface) const = 0;
};

class FrameHost {
public:
    virtual ~FrameHost() = default;

    virtual DisplayHandle handle() const = 0;
    virtual int drawable_width() const = 0;
    virtual int drawable_height() const = 0;
    virtual std::vector<std::string> font_dirs() const = 0;

    // Makes this window's Vulkan surfaces, or null to have the renderer make them from handle()
    virtual const VulkanSurfaceProvider *vulkan_surface_provider() const {
        return nullptr;
    }

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
