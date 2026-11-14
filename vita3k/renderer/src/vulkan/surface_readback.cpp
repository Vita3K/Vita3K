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

#include <gxm/functions.h>
#include <mem/functions.h>
#include <renderer/vulkan/gxm_to_vulkan.h>
#include <renderer/vulkan/surface_cache.h>
#include <renderer/vulkan/surface_readback.h>
#include <util/log.h>
#include <vkutil/readback.h>
extern "C" {
#include <libswscale/swscale.h>
}
namespace renderer::vulkan {
ColorSurfaceCacheInfo::~ColorSurfaceCacheInfo() {
    sws_freeContext(sws_context);
}
bool readback_cached_surface(MemState &mem, const ColorSurfaceCacheInfo &cached,
    const SceGxmColorSurface &surface, SurfaceReadbackContext &context) {
    if (!gpu_readback_supported() || context.memory_mapping || context.disable_surface_sync || !surface.data
        || surface.surfaceType != SCE_GXM_COLOR_SURFACE_LINEAR || !surface.width || !surface.height
        || cached.data.address() != surface.data.address())
        return false;
    const auto watch = cached.readback_watch;
    if (!watch || watch->invalidated)
        return false;
    struct FinishWatch {
        MemState &mem;
        std::shared_ptr<GpuReadbackWatch> watch;
        ~FinishWatch() { finish_gpu_readback_watch(mem, watch); }
    } finish{ mem, watch };
    const auto format = gxm::get_base_format(surface.colorFormat);
    uint32_t bpp;
    switch (format) {
    case SCE_GXM_COLOR_BASE_FORMAT_U5U6U5: bpp = 2; break;
    case SCE_GXM_COLOR_BASE_FORMAT_U8U8U8: bpp = 3; break;
    case SCE_GXM_COLOR_BASE_FORMAT_U8U8U8U8: bpp = 4; break;
    default:
        LOG_WARN_ONCE("Unmapped surface readback: unsupported color format {}", log_hex(format));
        return false;
    }
    const uint64_t stride = uint64_t(surface.strideInPixels) * bpp;
    if (cached.tiling != SurfaceTiling::Linear || cached.format != format
        || stride != cached.stride_bytes || surface.width > cached.original_width
        || surface.height > cached.original_height || surface.strideInPixels < surface.width
        || stride > UINT32_MAX)
        return false;
    // A cache entry can be larger than the latest rendering request. Crop the
    // active top-left extent rather than shrinking all of the older allocation.
    vkutil::Image active;
    active.destroy_on_deletion = false;
    active.image = cached.texture.image;
    active.width = uint32_t(surface.width * context.res_multiplier);
    active.height = uint32_t(surface.height * context.res_multiplier);
    active.format = cached.texture.format;
    active.layout = cached.texture.layout;
    if (!active.width || !active.height || active.width > cached.texture.width || active.height > cached.texture.height)
        return false;
    const uint64_t length = uint64_t(surface.height - 1) * stride + uint64_t(surface.width) * bpp;
    // Use the cache's original identity, not a fresh token for a replacement
    // allocation. Recheck before the GPU wait and atomically with the commit.
    const auto allocation = cached.readback_allocation;
    if (!matches_mem_allocation(mem, allocation, surface.data.address(), length))
        return false;
    std::vector<uint8_t> raw, pixels;
    const auto swizzle = color::translate_swizzle(surface.colorFormat);
    try {
        const std::lock_guard<std::mutex> lock(context.pool_mutex);
        if (!vkutil::read_image_to_host(context.physical, context.device, context.queue, context.queue_mutex,
                context.pool, context.allocator, active, surface.width, surface.height, raw)
            || !vkutil::convert_readback_pixels(active.format, swizzle, bpp, raw, pixels))
            return false;
    } catch (const vk::SystemError &error) {
        LOG_ERROR("Unmapped surface readback failed: {}", error.what());
        return false;
    }
    // Complete all conversion before making the first guest write. Row padding
    // and the existing CPU invalidation/protection records remain unchanged.
    if (!write_gpu_readback_rows(mem, surface.data.address(), pixels.data(),
            surface.width * bpp, surface.height, uint32_t(stride), &allocation, watch.get())) {
        LOG_ERROR("Unmapped surface readback: invalid guest destination");
        return false;
    }
    return true;
}

} // namespace renderer::vulkan
