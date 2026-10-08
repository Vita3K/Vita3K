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

#include <renderer/frame_host.h>

#include <SDL3/SDL_video.h>

#include <utility>

namespace sdl_frontend {

// Lets the renderer draw in an SDL window: Vulkan surfaces from SDL, or its OpenGL context
class FrameHost final : public renderer::FrameHost, public renderer::VulkanSurfaceProvider {
public:
    FrameHost(SDL_Window *window, SDL_GLContext *gl_context);

    renderer::DisplayHandle handle() const override;
    const renderer::VulkanSurfaceProvider *vulkan_surface_provider() const override;
    std::vector<const char *> instance_extensions() const override;
    VkSurfaceKHR create_surface(VkInstance instance) const override;
    void destroy_surface(VkInstance instance, VkSurfaceKHR surface) const override;
    int drawable_width() const override;
    int drawable_height() const override;
    std::vector<std::string> font_dirs() const override;
    void *get_proc_address(const char *name) const override;
    bool make_current() override;
    void done_current() override;
    void swap_buffers() override;
    bool set_vsync(bool enabled) override;
    void prepare_for_render_thread() override;
    void destroy_render_context() override;

private:
    // The window size in pixels, or 0x0 while Android has no surface to draw on
    std::pair<int, int> drawable_size() const;

    SDL_Window *window = nullptr;
    SDL_GLContext *gl_context = nullptr;
};

} // namespace sdl_frontend
