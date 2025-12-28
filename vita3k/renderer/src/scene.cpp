// Vita3K emulator project
// Copyright (C) 2025 Vita3K team
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

#include <gxm/functions.h>
#include <gxm/types.h>
#include <renderer/commands.h>
#include <renderer/driver_functions.h>
#include <renderer/state.h>
#include <renderer/types.h>

#include <renderer/gl/functions.h>
#include <renderer/gl/types.h>

#include <renderer/vulkan/functions.h>

#include <config/state.h>
#include <util/log.h>
#include <util/tracy.h>

#define DEBUG_FRAMEBUFFER 1

#if DEBUG_FRAMEBUFFER
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>
#endif

namespace renderer {
COMMAND(handle_set_context) {
    TRACY_FUNC_COMMANDS(handle_set_context);
    RenderTarget *rt = helper.pop<RenderTarget *>();
    const SceGxmColorSurface *color_surface = helper.pop<SceGxmColorSurface *>();
    const SceGxmDepthStencilSurface *depth_stencil_surface = helper.pop<SceGxmDepthStencilSurface *>();

    render_context->current_render_target = rt;

    if (color_surface && !color_surface->disabled) {
        render_context->record.color_surface = *color_surface;
    } else {
        render_context->record.color_surface.data = nullptr;
        render_context->record.color_surface.downscale = false;
    }

    delete color_surface;

    if (depth_stencil_surface && !depth_stencil_surface->disabled()) {
        render_context->record.depth_stencil_surface = *depth_stencil_surface;
    } else {
        render_context->record.depth_stencil_surface.depth_data.reset();
        render_context->record.depth_stencil_surface.stencil_data.reset();
    }

    delete depth_stencil_surface;

    switch (renderer.current_backend) {
    case Backend::OpenGL:
        gl::set_context(dynamic_cast<gl::GLState &>(renderer), *reinterpret_cast<gl::GLContext *>(render_context), mem, reinterpret_cast<const gl::GLRenderTarget *>(rt), features);
        break;

    case Backend::Vulkan:
        vulkan::set_context(*reinterpret_cast<vulkan::VKContext *>(render_context), mem, reinterpret_cast<vulkan::VKRenderTarget *>(rt), features);
        break;

    default:
        REPORT_MISSING(renderer.current_backend);
        break;
    }
}

COMMAND(handle_sync_surface_data) {
    TRACY_FUNC_COMMANDS(handle_sync_surface_data);

    const SceGxmNotification vertex_notification = helper.pop<SceGxmNotification>();
    const SceGxmNotification fragment_notification = helper.pop<SceGxmNotification>();
    // with memory mapping, notifications are signaled another way
    // also don't try to signal if there are no notifications
    bool were_notifications_signaled = renderer.features.enable_memory_mapping
        || (!vertex_notification.address && !fragment_notification.address);

    auto signal_notifications = [&]() {
        if (were_notifications_signaled)
            return;

        were_notifications_signaled = true;
        // signal the notification now
        std::unique_lock<std::mutex> lock(renderer.notification_mutex);

        if (vertex_notification.address)
            *vertex_notification.address.get(mem) = vertex_notification.value;
        if (fragment_notification.address)
            *fragment_notification.address.get(mem) = fragment_notification.value;

        // unlocking before a notify should be faster
        lock.unlock();
        renderer.notification_ready.notify_all();
    };

    if (renderer.disable_surface_sync)
        // do it as soon as possible
        signal_notifications();

    if (renderer.current_backend == Backend::Vulkan) {
        // TODO: put this in a function
        vulkan::VKContext *context = reinterpret_cast<vulkan::VKContext *>(renderer.context);
        if (context->is_recording)
            context->stop_recording(vertex_notification, fragment_notification);
    }

    SceGxmColorSurface *surface = &render_context->record.color_surface;
    if (helper.cmd->status) {
        surface = helper.pop<SceGxmColorSurface *>();
        if (!surface) {
            signal_notifications();
            complete_command(renderer, helper, 1);
            return;
        }
    }

    // additional check to make sure we never try to perform surface sync on OpenGL with a non-integer resolution multiplier
    if (renderer.current_backend == Backend::OpenGL && static_cast<int>(renderer.res_multiplier * 4.0f) % 4 != 0)
        renderer.disable_surface_sync = true;

    if (renderer.disable_surface_sync || renderer.current_backend == Backend::Vulkan) {
        if (helper.cmd->status) {
            complete_command(renderer, helper, 0);
        }
        signal_notifications();
        return;
    }

#ifndef __ANDROID__

    const size_t width = surface->width;
    const size_t height = surface->height;
    const size_t stride_in_pixels = surface->strideInPixels;
    const Address data = surface->data.address();
    uint32_t *const pixels = Ptr<uint32_t>(data).get(mem);

    // We protect the data to track syncing. If this is called then the data is definitely protected somehow.
    // We just unprotect and reprotect again :D
    const std::size_t total_size = height * gxm::get_stride_in_bytes(surface->colorFormat, stride_in_pixels);

    switch (renderer.current_backend) {
    case Backend::OpenGL:
        if (helper.cmd->status) {
            gl::lookup_and_get_surface_data(static_cast<gl::GLState &>(renderer), mem, *surface);
        } else {
            gl::get_surface_data(static_cast<gl::GLState &>(renderer), *reinterpret_cast<gl::GLContext *>(render_context), pixels, *surface);
        }
        break;

    case Backend::Vulkan:
        break;

    default:
        REPORT_MISSING(renderer.current_backend);
        break;
    }

#if DEBUG_FRAMEBUFFER
    if (data != 0 && config.color_surface_debug) {
        const std::string filename = fmt::format("color_surface_0x{:X}.png", data);

        // Assuming output is RGBA
        const int result = stbi_write_png(filename.c_str(), width, height, 4, pixels, stride_in_pixels * 4);

        if (!result) {
            LOG_TRACE("Fail to save color surface 0x{:X}", data);
        }
    }
#endif
#endif

    if (helper.cmd->status) {
        complete_command(renderer, helper, 0);
    }

    signal_notifications();
}

COMMAND(handle_mid_scene_flush) {
    TRACY_FUNC_COMMANDS(handle_mid_scene_flush);

    if (!renderer.features.enable_memory_mapping) {
        // handle it like a simple notification
        cmd_handle_notification(renderer, mem, config, helper, features, render_context);
        return;
    }

    const SceGxmNotification notification = helper.pop<SceGxmNotification>();
    if (renderer.current_backend == Backend::Vulkan) {
        vulkan::mid_scene_flush(*reinterpret_cast<vulkan::VKContext *>(render_context), notification);
    }
}

COMMAND(handle_draw) {
    TRACY_FUNC_COMMANDS(handle_draw);
    SceGxmPrimitiveType type = helper.pop<SceGxmPrimitiveType>();
    SceGxmIndexFormat format = helper.pop<SceGxmIndexFormat>();
    Ptr<const void> indices = helper.pop<Ptr<const void>>();
    const std::uint32_t count = helper.pop<const std::uint32_t>();
    const std::uint32_t instance_count = helper.pop<const std::uint32_t>();

    switch (renderer.current_backend) {
    case Backend::OpenGL:
        gl::draw(dynamic_cast<gl::GLState &>(renderer), *reinterpret_cast<gl::GLContext *>(render_context),
            features, type, format, indices.cast<void>().get(mem), count, instance_count, mem, config);
        break;

    case Backend::Vulkan:
        vulkan::draw(*reinterpret_cast<vulkan::VKContext *>(render_context), type, format, indices.cast<void>(),
            count, instance_count, mem, config);
        break;

    default:
        REPORT_MISSING(renderer.current_backend);
        break;
    }
}
} // namespace renderer
