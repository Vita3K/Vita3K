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

#include "frame_host.h"

#include <util/log.h>

#include <SDL3/SDL_vulkan.h>

#ifdef __ANDROID__
#include <renderer/vulkan/screen_renderer.h>
#endif

#include <cstdlib>

namespace sdl_frontend {

FrameHost::FrameHost(SDL_Window *window, SDL_GLContext *gl_context)
    : window(window)
    , gl_context(gl_context) {
}

renderer::DisplayHandle FrameHost::handle() const {
    return {};
}

const renderer::VulkanSurfaceProvider *FrameHost::vulkan_surface_provider() const {
    return this;
}

std::vector<const char *> FrameHost::instance_extensions() const {
    Uint32 count = 0;
    const char *const *extensions = SDL_Vulkan_GetInstanceExtensions(&count);
    if (!extensions) {
        LOG_ERROR("SDL_Vulkan_GetInstanceExtensions failed: {}", SDL_GetError());
        return {};
    }
    return { extensions, extensions + count };
}

VkSurfaceKHR FrameHost::create_surface(VkInstance instance) const {
    if (!window) {
        LOG_WARN("SDL window is not ready yet; deferring Vulkan surface recreation");
        return VK_NULL_HANDLE;
    }

    VkSurfaceKHR surface = VK_NULL_HANDLE;
    if (!SDL_Vulkan_CreateSurface(window, instance, nullptr, &surface)) {
        LOG_WARN("SDL_Vulkan_CreateSurface failed: {}", SDL_GetError());
        return VK_NULL_HANDLE;
    }
    return surface;
}

void FrameHost::destroy_surface(VkInstance instance, VkSurfaceKHR surface) const {
    SDL_Vulkan_DestroySurface(instance, surface, nullptr);
}

std::pair<int, int> FrameHost::drawable_size() const {
#ifdef __ANDROID__
    if (!renderer::vulkan::has_android_surface())
        return { 0, 0 };
#endif
    int width = 960;
    int height = 544;
    SDL_GetWindowSizeInPixels(window, &width, &height);
    return { width, height };
}

int FrameHost::drawable_width() const {
    return drawable_size().first;
}

int FrameHost::drawable_height() const {
    return drawable_size().second;
}

std::vector<std::string> FrameHost::font_dirs() const {
#ifdef __ANDROID__
    return { "/system/fonts/" };
#elif defined(_WIN32)
    const char *windir = std::getenv("WINDIR");
    return { std::string(windir ? windir : "C:\\Windows") + "\\Fonts\\" };
#elif defined(__APPLE__)
    const char *home = std::getenv("HOME");
    std::vector<std::string> dirs{ "/System/Library/Fonts/", "/Library/Fonts/" };
    if (home)
        dirs.push_back(std::string(home) + "/Library/Fonts/");
    return dirs;
#else
    const char *home = std::getenv("HOME");
    std::vector<std::string> dirs{ "/usr/local/share/fonts/", "/usr/share/fonts/" };
    if (home)
        dirs.insert(dirs.begin(), std::string(home) + "/.local/share/fonts/");
    return dirs;
#endif
}

void *FrameHost::get_proc_address(const char *name) const {
    return reinterpret_cast<void *>(SDL_GL_GetProcAddress(name));
}

bool FrameHost::make_current() {
    if (!gl_context || !*gl_context)
        return false;
    return SDL_GL_MakeCurrent(window, *gl_context);
}

void FrameHost::done_current() {
    SDL_GL_MakeCurrent(window, nullptr);
}

void FrameHost::swap_buffers() {
    SDL_GL_SwapWindow(window);
}

bool FrameHost::set_vsync(bool enabled) {
    return SDL_GL_SetSwapInterval(enabled ? 1 : 0);
}

void FrameHost::prepare_for_render_thread() {
    if (gl_context && *gl_context)
        SDL_GL_MakeCurrent(window, nullptr);
}

void FrameHost::destroy_render_context() {
    if (!gl_context || !*gl_context)
        return;

    SDL_GL_DestroyContext(*gl_context);
    *gl_context = nullptr;
}

} // namespace sdl_frontend
