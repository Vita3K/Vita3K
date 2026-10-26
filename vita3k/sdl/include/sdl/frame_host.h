// Vita3K emulator project
// Copyright (C) 2026 Vita3K team
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.

#pragma once

#include <renderer/frame_host.h>

#include <SDL3/SDL_video.h>

#include <utility>

namespace sdl {

class FrameHost : public renderer::FrameHost, public renderer::VulkanSurfaceProvider {
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

protected:
    SDL_Window *window;
    SDL_GLContext *gl_context;

private:
    std::pair<int, int> drawable_size() const;
};

} // namespace sdl