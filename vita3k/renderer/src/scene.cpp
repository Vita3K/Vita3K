// Vita3K emulator project
// Copyright (C) 2023 Vita3K team
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
#include <renderer/functions.h>
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

    if (rt) {
        render_context->current_render_target = rt;
    }

    if (color_surface && !color_surface->disabled) {
        render_context->record.color_surface = *color_surface;
    } else {
        render_context->record.color_surface.data = nullptr;
    }

    if (color_surface)
        delete color_surface;

    if (depth_stencil_surface) {
        render_context->record.depth_stencil_surface = *depth_stencil_surface;
        delete depth_stencil_surface;
    } else {
        static const SceGxmDepthStencilSurface default_ds{
            .zlsControl = 0,
            .depthData = Ptr<void>(0),
            .stencilData = Ptr<void>(0),
            .backgroundDepth = 1.0f,
            .control = { SceGxmDepthStencilControl::mask_bit }
        };
        render_context->record.depth_stencil_surface = default_ds;
    }

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

    if ((vertex_notification.address || fragment_notification.address)
        && !renderer.features.support_memory_mapping) {
        // signal the notification now
        std::unique_lock<std::mutex> lock(renderer.notification_mutex);

        if (vertex_notification.address)
            *vertex_notification.address.get(mem) = vertex_notification.value;
        if (fragment_notification.address)
            *fragment_notification.address.get(mem) = fragment_notification.value;

        // unlocking before a notify should be faster
        lock.unlock();
        renderer.notification_ready.notify_all();
    }

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
            complete_command(renderer, helper, 1);
            return;
        }
    }

    if (renderer.disable_surface_sync) {
        if (helper.cmd->status)
            complete_command(renderer, helper, 0);
        return;
    }

    const size_t width = surface->width;
    const size_t height = surface->height;
    const size_t stride_in_pixels = surface->strideInPixels;
    const Address data = surface->data.address();
    uint32_t *const pixels = Ptr<uint32_t>(data).get(mem);

    // We protect the data to track syncing. If this is called then the data is definitely protected somehow.
    // We just unprotect and reprotect again :D
    const std::size_t total_size = height * gxm::get_stride_in_bytes(surface->colorFormat, stride_in_pixels);

    open_access_parent_protect_segment(mem, data);
    unprotect_inner(mem, data, total_size);

    switch (renderer.current_backend) {
    case Backend::OpenGL:
        if (helper.cmd->status) {
            gl::lookup_and_get_surface_data(static_cast<gl::GLState &>(renderer), mem, *surface);
        } else {
            gl::get_surface_data(static_cast<gl::GLState &>(renderer), *reinterpret_cast<gl::GLContext *>(render_context), pixels, *surface);
        }
        break;

    case Backend::Vulkan:
        // not implemented for now
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

    // Need to reprotect. In the case of explicit get, 100% chance it will be unlock later anyway.
    // No need to bother. Assumption of course.
    if (!helper.cmd->status && is_protecting(mem, data)) {
        protect_inner(mem, data, total_size, MEM_PERM_NONE);
    }

    close_access_parent_protect_segment(mem, data);

    if (helper.cmd->status) {
        complete_command(renderer, helper, 0);
    }
}

COMMAND(handle_draw) {
    TRACY_FUNC_COMMANDS(handle_draw);
    SceGxmPrimitiveType type = helper.pop<SceGxmPrimitiveType>();
    SceGxmIndexFormat format = helper.pop<SceGxmIndexFormat>();
    void *indicies = helper.pop<void *>();
    const std::uint32_t count = helper.pop<const std::uint32_t>();
    const std::uint32_t instance_count = helper.pop<const std::uint32_t>();

    switch (renderer.current_backend) {
    case Backend::OpenGL:
        gl::draw(dynamic_cast<gl::GLState &>(renderer), *reinterpret_cast<gl::GLContext *>(render_context),
            features, type, format, indicies, count, instance_count, mem, base_path, title_id, self_name, config);
        break;

    case Backend::Vulkan:
        vulkan::draw(*reinterpret_cast<vulkan::VKContext *>(render_context), type, format, indicies,
            count, instance_count, mem, config);
        break;

    default:
        REPORT_MISSING(renderer.current_backend);
        break;
    }
}

COMMAND(handle_transfer_copy) {
    TRACY_FUNC_COMMANDS(handle_transfer_copy);
    const uint32_t colorKeyValue = helper.pop<uint32_t>();
    const uint32_t colorKeyMask = helper.pop<uint32_t>();
    const SceGxmTransferColorKeyMode colorKeyMode = helper.pop<SceGxmTransferColorKeyMode>();
    const SceGxmTransferImage *images = helper.pop<SceGxmTransferImage *>();
    const SceGxmTransferImage *src = &images[0];
    const SceGxmTransferImage *dest = &images[1];
    const SceGxmTransferType src_type = helper.pop<SceGxmTransferType>();
    const SceGxmTransferType dst_type = helper.pop<SceGxmTransferType>();

    if (src_type == dst_type) {
        const auto src_bpp = gxm::get_bits_per_pixel(src->format);
        const auto dest_bpp = gxm::get_bits_per_pixel(dest->format);
        const uint32_t src_bytes_per_pixel = (src_bpp + 7) >> 3;
        const uint32_t dest_bytes_per_pixel = (dest_bpp + 7) >> 3;

        for (uint32_t x = 0; x < src->width; x++) {
            for (uint32_t y = 0; y < src->height; y++) {
                // Set offset of source and destination
                const auto src_offset = ((x + src->x) * src_bytes_per_pixel) + ((y + src->y) * src->stride);
                const auto dest_offset = ((x + dest->x) * dest_bytes_per_pixel) + ((y + dest->y) * dest->stride);

                // Set pointer of source and destination
                const auto src_ptr = (uint8_t *)src->address.get(mem) + src_offset;
                auto dest_ptr = (uint8_t *)dest->address.get(mem) + dest_offset;

                // Set color of source
                const auto src_color = *(uint32_t *)src_ptr;

                // Copy result in destination depending color Key
                switch (colorKeyMode) {
                case SCE_GXM_TRANSFER_COLORKEY_NONE:
                    memcpy(dest_ptr, src_ptr, dest_bytes_per_pixel);
                    break;
                case SCE_GXM_TRANSFER_COLORKEY_PASS:
                    if ((src_color & colorKeyMask) == colorKeyValue)
                        memcpy(dest_ptr, src_ptr, dest_bytes_per_pixel);
                    break;
                case SCE_GXM_TRANSFER_COLORKEY_REJECT:
                    if ((src_color & colorKeyMask) != colorKeyValue)
                        memcpy(dest_ptr, src_ptr, dest_bytes_per_pixel);
                    break;
                default: break;
                }
            }
        }
    } else
        LOG_WARN("No convertion of SceGxmTransferType support yet");

    // TODO: handle case where dest is a cached surface

    delete[] images;
}

COMMAND(handle_transfer_downscale) {
    TRACY_FUNC_COMMANDS(handle_transfer_downscale);
    const SceGxmTransferImage *src = helper.pop<SceGxmTransferImage *>();
    const SceGxmTransferImage *dest = helper.pop<SceGxmTransferImage *>();

    const auto src_bpp = gxm::get_bits_per_pixel(src->format);
    const auto dest_bpp = gxm::get_bits_per_pixel(dest->format);
    const uint32_t src_bytes_per_pixel = (src_bpp + 7) >> 3;
    const uint32_t dest_bytes_per_pixel = (dest_bpp + 7) >> 3;

    for (uint32_t y = 0; y < src->height; y += 2) {
        for (uint32_t x = 0; x < src->width; x += 2) {
            // Set offset of source and destination
            const auto src_offset = ((x + src->x) * src_bytes_per_pixel) + ((y + src->y) * src->stride);
            const auto dest_offset = (x / 2 + dest->x) * dest_bytes_per_pixel + (y / 2 + dest->y) * dest->stride;

            // Set pointer of source and destination
            const auto src_ptr = (uint8_t *)src->address.get(mem) + src_offset;
            auto dest_ptr = (uint8_t *)dest->address.get(mem) + dest_offset;

            // Copy result in destination
            memcpy(dest_ptr, src_ptr, dest_bytes_per_pixel);
        }
    }

    // TODO: handle case where dest is a cached surface

    delete src;
    delete dest;
}

COMMAND(handle_transfer_fill) {
    TRACY_FUNC_COMMANDS(handle_transfer_fill);
    const uint32_t fill_color = helper.pop<uint32_t>();
    const SceGxmTransferImage *dest = helper.pop<SceGxmTransferImage *>();

    const auto bpp = gxm::get_bits_per_pixel(dest->format);

    const uint32_t bytes_per_pixel = (bpp + 7) >> 3;
    for (uint32_t y = 0; y < dest->height; y++) {
        for (uint32_t x = 0; x < dest->width; x++) {
            // Set offset of destination
            const auto dest_offset = ((x + dest->x) * bytes_per_pixel) + ((y + dest->y) * dest->stride);

            // Set pointer of destination
            auto dest_ptr = (uint8_t *)dest->address.get(mem) + dest_offset;

            // Fill color in destination
            memcpy(dest_ptr, &fill_color, bytes_per_pixel);
        }
    }

    // TODO: handle case where dest is a cached surface

    delete dest;
}

} // namespace renderer
