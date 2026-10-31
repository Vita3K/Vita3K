// Vita3K emulator project
// Copyright (C) 2026 Vita3K team
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.

#include <sdl/sdl_window.h>

#include <renderer/types.h>
#include <util/log.h>

#include <SDL3/SDL_properties.h>

#include <algorithm>

namespace sdl {

namespace {

Viewport viewport;

bool set_gl_context_attributes() {
#ifdef __ANDROID__
    constexpr SDL_GLProfile profile = SDL_GL_CONTEXT_PROFILE_ES;
    constexpr int major = 3;
    constexpr int minor = 2;
#else
    constexpr SDL_GLProfile profile = SDL_GL_CONTEXT_PROFILE_CORE;
    constexpr int major = 4;
    constexpr int minor = 3;
#endif
    const bool profile_set = SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, profile);
    const bool major_set = SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, major);
    const bool minor_set = SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, minor);
    const bool double_buffer_set = SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    const bool depth_set = SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 0);
    bool debug_set = true;
#ifndef NDEBUG
    debug_set = SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_DEBUG_FLAG);
#endif
    return profile_set && major_set && minor_set && double_buffer_set && depth_set && debug_set;
}

} // namespace

bool update_viewport(SDL_Window *window, bool stretch_display_area, bool fullscreen_hd_res_pixel_perfect,
    bool fullscreen) {
    if (!window) {
        LOG_ERROR("Cannot update the SDL viewport without a window.");
        return false;
    }

    Viewport updated;
    int width = 0;
    int height = 0;
    SDL_GetWindowSize(window, &width, &height);
    updated.window_width = static_cast<float>(width);
    updated.window_height = static_cast<float>(height);
    SDL_GetWindowSizeInPixels(window, &width, &height);
    updated.drawable_width = static_cast<float>(width);
    updated.drawable_height = static_cast<float>(height);
    updated.system_dpi_scale = SDL_GetWindowPixelDensity(window);
    updated.manual_dpi_scale = SDL_GetDisplayContentScale(SDL_GetDisplayForWindow(window));
    if (updated.system_dpi_scale <= 0.f || updated.manual_dpi_scale <= 0.f) {
        LOG_ERROR("SDL returned an invalid DPI scale while updating the viewport.");
        return false;
    }

    if (height > 0) {
        const float window_aspect = static_cast<float>(width) / static_cast<float>(height);
        const float vita_aspect = static_cast<float>(VITA_VIEWPORT_WIDTH) / VITA_VIEWPORT_HEIGHT;
        const bool pixel_perfect_fullscreen = fullscreen_hd_res_pixel_perfect && fullscreen
            && width % VITA_VIEWPORT_WIDTH == 0 && height % (VITA_VIEWPORT_HEIGHT - 4) == 0;
        if (stretch_display_area && !pixel_perfect_fullscreen) {
            updated.logical_width = updated.window_width;
            updated.logical_height = updated.window_height;
            updated.drawable_view_width = updated.drawable_width;
            updated.drawable_view_height = updated.drawable_height;
        } else if (window_aspect > vita_aspect && !pixel_perfect_fullscreen) {
            updated.logical_width = updated.window_height * vita_aspect;
            updated.logical_height = updated.window_height;
            updated.logical_x = (updated.window_width - updated.logical_width) / 2.f;
            updated.drawable_view_width = updated.drawable_height * vita_aspect;
            updated.drawable_x = (updated.drawable_width - updated.drawable_view_width) / 2.f;
        } else {
            updated.logical_width = updated.window_width;
            updated.logical_height = updated.window_width / vita_aspect;
            updated.logical_y = (updated.window_height - updated.logical_height) / 2.f;
            updated.drawable_view_width = updated.drawable_width;
            updated.drawable_view_height = updated.drawable_width / vita_aspect;
            updated.drawable_y = (updated.drawable_height - updated.drawable_view_height) / 2.f;
        }
        updated.scale_x = updated.logical_width / static_cast<float>(VITA_VIEWPORT_WIDTH) / updated.manual_dpi_scale;
        updated.scale_y = updated.logical_height / static_cast<float>(VITA_VIEWPORT_HEIGHT) / updated.manual_dpi_scale;
    }

    viewport = updated;
    return true;
}

const Viewport &get_viewport() {
    return viewport;
}

SDL_Window *create_window(const char *title, renderer::Backend backend, SDL_WindowFlags flags) {
    const float scale = (flags & SDL_WINDOW_HIGH_PIXEL_DENSITY)
        ? std::max(1.f, SDL_GetDisplayContentScale(SDL_GetPrimaryDisplay()))
        : 1.f;
    const int width = static_cast<int>(VITA_VIEWPORT_WIDTH * scale);
    const int height = static_cast<int>(VITA_VIEWPORT_HEIGHT * scale);

    SDL_WindowFlags backend_flag = 0;
    switch (backend) {
    case renderer::Backend::OpenGL:
        if (!set_gl_context_attributes())
            return nullptr;
        backend_flag = SDL_WINDOW_OPENGL;
        break;
    case renderer::Backend::Vulkan:
        backend_flag = SDL_WINDOW_VULKAN;
        break;
    default:
        LOG_ERROR("Unsupported renderer backend for SDL window creation.");
        return nullptr;
    }

    flags &= ~(SDL_WINDOW_OPENGL | SDL_WINDOW_VULKAN);
    flags |= backend_flag;

    const SDL_PropertiesID properties = SDL_CreateProperties();
    if (!properties)
        return nullptr;

    bool properties_set = SDL_SetStringProperty(properties, SDL_PROP_WINDOW_CREATE_TITLE_STRING, title)
        && SDL_SetNumberProperty(properties, SDL_PROP_WINDOW_CREATE_WIDTH_NUMBER, width)
        && SDL_SetNumberProperty(properties, SDL_PROP_WINDOW_CREATE_HEIGHT_NUMBER, height)
        && SDL_SetNumberProperty(properties, SDL_PROP_WINDOW_CREATE_FLAGS_NUMBER, flags);
    if (properties_set && backend == renderer::Backend::OpenGL)
        properties_set = SDL_SetBooleanProperty(properties, SDL_PROP_WINDOW_CREATE_OPENGL_BOOLEAN, true);

    SDL_Window *window = properties_set ? SDL_CreateWindowWithProperties(properties) : nullptr;
    SDL_DestroyProperties(properties);
    if (window)
        update_viewport(window, false, false, (flags & SDL_WINDOW_FULLSCREEN) != 0);
    return window;
}

} // namespace sdl
