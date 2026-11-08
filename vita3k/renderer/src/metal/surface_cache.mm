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

#include "surface_cache.h"

#include "gxm_to_metal.h"
#include "mtl_types.h"

#include <gxm/functions.h>
#include <util/align.h>
#include <util/log.h>

#include <array>

namespace renderer::metal {

static void protect_surface(MemState &mem, ColorSurfaceCacheInfo &info) {
    uint32_t addr_start = align(info.data.address(), KiB(4));
    uint32_t addr_end = align_down(info.data.address() + info.total_bytes, KiB(4));
    const bool small_surface = addr_start >= addr_end;
    if (small_surface) {
        // we still need to protect something, even if it's not completely accurate
        addr_start = align_down(info.data.address(), KiB(4));
        addr_end = align(info.data.address() + info.total_bytes, KiB(4));
    }

    // Don't track dirty for small surfaces to avoid false positives from unrelated writes
    std::shared_ptr<bool> dirty = small_surface ? nullptr : info.dirty;

    add_protect(mem, addr_start, addr_end - addr_start, MemPerm::ReadOnly,
        [dirty](Address, bool write) {
            if (write && dirty)
                *dirty = true;
            return true;
        });
}

void SurfaceCache::destroy_surface(ColorSurfaceCacheInfo &info) {
    // the textures are kept alive by Metal while they are in use
    info.casted_textures.clear();
    info.alternate_view = nil;
    info.gamma_view = nil;
    info.texture = nil;
}

void SurfaceCache::destroy_surface(DepthStencilSurfaceCacheInfo &info) {
    info.read_surfaces.clear();
    info.stencil_view = nil;
    info.texture = nil;
}

SurfaceCache::SurfaceCache(MetalState &state)
    : state(state) {
    color_surface_queue.init(max_surfaces_allowed);
    ds_surface_queue.init(max_surfaces_allowed);
}

void SurfaceCache::cleanup() {
    for (auto &item : color_surface_queue.items)
        destroy_surface(item.content);

    for (auto &item : ds_surface_queue.items)
        destroy_surface(item.content);

    color_address_lookup.clear();
    depth_address_lookup.clear();
    stencil_address_lookup.clear();
    target = nullptr;
}

static MTLPixelFormat get_surface_format(MetalState &state, SceGxmColorBaseFormat base_format, bool is_srgb) {
    MTLPixelFormat format = color::translate_format(base_format, state.device->support_packed16);
    if (is_srgb) {
        if (format == MTLPixelFormatRGBA8Unorm) {
            format = MTLPixelFormatRGBA8Unorm_sRGB;
        } else {
            LOG_WARN_ONCE("Trying to use gamma correction with non-compatible format {}", format_name(format));
        }
    }
    return format;
}

static id<MTLTexture> get_format_view(ColorSurfaceCacheInfo &info, MTLPixelFormat format) {
    if (format == info.pixel_format)
        return info.texture;
    if (!info.gamma_view || info.gamma_view.pixelFormat != format)
        info.gamma_view = create_texture_view(info.texture, format, identity_swizzle);
    return info.gamma_view;
}

SurfaceRetrieveResult SurfaceCache::retrieve_color_surface_for_framebuffer(MemState &mem, SceGxmColorSurface *color) {
    MetalDevice &dev = *state.device;

    // Create the key to access the cache struct
    const uint32_t address = color->data.address();

    const uint32_t original_width = color->width;
    const uint32_t original_height = color->height;

    const uint32_t width = static_cast<uint32_t>(original_width * state.res_multiplier);
    const uint32_t height = static_cast<uint32_t>(original_height * state.res_multiplier);

    bool overlap = true;

    // Of course, this works under the assumption that range must be unique :D
    auto ite = color_address_lookup.upper_bound(address);
    if (ite == color_address_lookup.begin())
        // no match
        overlap = false;
    else
        --ite;
    // ite is now the first item with an address lower or equal to key

    overlap = (overlap && (ite->first + ite->second->total_bytes) > address);

    const SceGxmColorBaseFormat base_format = gxm::get_base_format(color->colorFormat);
    const MTLPixelFormat format = get_surface_format(state, base_format, color->gamma != 0);

    SurfaceTiling tiling;
    if (color->surfaceType == SCE_GXM_COLOR_SURFACE_LINEAR)
        tiling = SurfaceTiling::Linear;
    else if (color->surfaceType == SCE_GXM_COLOR_SURFACE_SWIZZLED)
        tiling = SurfaceTiling::Swizzled;
    else
        tiling = SurfaceTiling::Tiled;

    const uint32_t bytes_per_stride = color->strideInPixels * gxm::bits_per_pixel(base_format) / 8;
    const uint32_t total_surface_size = bytes_per_stride * original_height;

    if (overlap) {
        ColorSurfaceCacheInfo &info = *ite->second;

        const bool addr_in_range_of_cache = ((address + total_surface_size) <= (ite->first + info.total_bytes + 4));
        const bool cache_probably_freed = (ite->first != address) && addr_in_range_of_cache;
        const bool surface_extent_changed = info.height < height || bytes_per_stride != info.stride_bytes || tiling != info.tiling;
        bool surface_stat_changed = false;

        if (ite->first == address)
            surface_stat_changed = surface_extent_changed || info.width < width || base_format != info.format;

        const bool invalidated = cache_probably_freed || surface_stat_changed || !addr_in_range_of_cache;
        if (invalidated) {
            destroy_surface(info);
            color_address_lookup.erase(ite);
            color_surface_queue.set_as_lru(&info);
        } else {
            color_surface_queue.set_as_mru(&info);

            if (info.data && *info.dirty)
                protect_surface(mem, info);
            *info.dirty = false;

            // if this surface has not been rendered to for the last 60 frames, consider it is not safe not to render all shaders to it
            constexpr uint64_t big_delay_between_frames = 60;
            dev.pipeline_cache.can_use_deferred_compilation = dev.frame_timestamp - info.last_frame_rendered < big_delay_between_frames;
            info.last_frame_rendered = dev.frame_timestamp;

            return { get_format_view(info, format), info.texture, false };
        }
    }

    // get the least recently used (probably unused) color surface
    ColorSurfaceCacheInfo &info_added = *color_surface_queue.get_lru();
    if (info_added.texture)
        destroy_surface(info_added);
    if (info_added.data)
        color_address_lookup.erase(info_added.data.address());

    color_surface_queue.set_as_mru(&info_added);
    info_added.last_frame_rendered = dev.frame_timestamp;

    color_address_lookup[address] = &info_added;

    info_added.width = width;
    info_added.height = height;
    info_added.original_width = original_width;
    info_added.original_height = original_height;
    info_added.stride_bytes = bytes_per_stride;
    info_added.data = color->data;
    info_added.total_bytes = total_surface_size;
    info_added.format = base_format;
    info_added.tiling = tiling;
    // only remember the swizzle here, it will be useful if we get to present or sample from this image with a different swizzle
    info_added.swizzle = color::translate_swizzle(color->colorFormat);

    // the texture is stored with the linear format, an sRGB view is used when needed
    info_added.pixel_format = is_srgb_format(format) ? MTLPixelFormatRGBA8Unorm : format;
    const MTLTextureUsage usage = MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead | MTLTextureUsagePixelFormatView;
    info_added.texture = create_texture(dev.device, info_added.pixel_format, width, height, usage, 1, false, 1, MTLStorageModePrivate, "color surface");
    info_added.alternate_view = nil;
    info_added.gamma_view = nil;
    info_added.dirty = std::make_shared<bool>(false);

    protect_surface(mem, info_added);

    // it's not impossible that this surface will be rendered once and only used after, so do not skip any shader on it
    dev.pipeline_cache.can_use_deferred_compilation = false;

    return { get_format_view(info_added, format), info_added.texture, true };
}

std::optional<TextureLookupResult> SurfaceCache::retrieve_color_surface_as_texture(const SceGxmTexture &texture, const SceGxmColorBaseFormat base_format, TextureViewport *texture_viewport) {
    MetalDevice &dev = *state.device;

    // Create the key to access the cache struct
    const uint32_t address = (texture.data_addr << 2);

    const uint32_t original_width = gxm::get_width(texture);
    const uint32_t original_height = gxm::get_height(texture);

    const uint32_t width = static_cast<uint32_t>(original_width * state.res_multiplier);
    const uint32_t height = static_cast<uint32_t>(original_height * state.res_multiplier);

    bool overlap = true;
    auto ite = color_address_lookup.upper_bound(address);
    if (ite == color_address_lookup.begin())
        // no match
        overlap = false;
    else
        --ite;

    overlap = (overlap && (ite->first + ite->second->total_bytes) > address);

    if (!overlap)
        return std::nullopt;

    if (*ite->second->dirty)
        // Guest wrote to the surface backing memory since it was rendered, so GPU data is stale.
        return std::nullopt;

    const MTLTextureSwizzleChannels swizzle = texture::translate_swizzle(gxm::get_format(texture));
    const MTLPixelFormat format = get_surface_format(state, base_format, texture.gamma_mode != 0);

    uint32_t stride_bytes = 0;
    SurfaceTiling tiling = SurfaceTiling::Swizzled;
    if (texture.texture_type() == SCE_GXM_TEXTURE_LINEAR_STRIDED) {
        stride_bytes = gxm::get_stride_in_bytes(texture);
        tiling = SurfaceTiling::Linear;
    } else {
        uint32_t pixel_stride = original_width;
        switch (texture.texture_type()) {
        case SCE_GXM_TEXTURE_LINEAR:
            // when the texture is linear, the stride should be aligned to 8 pixels
            tiling = SurfaceTiling::Linear;
            pixel_stride = align(pixel_stride, 8);
            break;
        case SCE_GXM_TEXTURE_TILED:
            // tiles are 32x32
            tiling = SurfaceTiling::Tiled;
            pixel_stride = align(pixel_stride, 32);
            break;
        case SCE_GXM_TEXTURE_SWIZZLED_ARBITRARY:
            pixel_stride = next_power_of_two(pixel_stride);
            break;
        default:
            break;
        }
        stride_bytes = pixel_stride * gxm::bits_per_pixel(base_format) / 8;
    }
    const uint32_t total_surface_size = stride_bytes * original_height;

    ColorSurfaceCacheInfo &info = *ite->second;

    const bool expanded_format = (info.format != base_format)
        && (info.format == SCE_GXM_COLOR_BASE_FORMAT_U8U8U8 || base_format == SCE_GXM_COLOR_BASE_FORMAT_U8U8U8
            || (!dev.support_packed16 && (base_format == SCE_GXM_COLOR_BASE_FORMAT_U4U4U4U4 || base_format == SCE_GXM_COLOR_BASE_FORMAT_U5U6U5 || base_format == SCE_GXM_COLOR_BASE_FORMAT_U1U5U5U5))
            || (!dev.support_packed16 && (info.format == SCE_GXM_COLOR_BASE_FORMAT_U4U4U4U4 || info.format == SCE_GXM_COLOR_BASE_FORMAT_U5U6U5 || info.format == SCE_GXM_COLOR_BASE_FORMAT_U1U5U5U5)));
    if (expanded_format)
        // Expanded surfaces cannot be reinterpreted as a different guest format.
        return std::nullopt;

    if (tiling != info.tiling || info.stride_bytes != stride_bytes)
        return std::nullopt;

    // Check if we can use this surface
    const bool addr_in_range_of_cache = ((address + total_surface_size) <= (ite->first + info.total_bytes + 4));

    if (ite->first != address && !addr_in_range_of_cache)
        // persona 4 sample from the top of a texture while the bottom wasn't rendered to, the fact that both the surface and
        // the texture start at the same location should be enough
        return std::nullopt;

    const uint32_t bytes_per_pixel_requested = gxm::bits_per_pixel(base_format) / 8;
    const uint32_t bytes_per_pixel_in_store = gxm::bits_per_pixel(info.format) / 8;

    if (std::max(bytes_per_pixel_requested, bytes_per_pixel_in_store) % std::min(bytes_per_pixel_requested, bytes_per_pixel_in_store) != 0)
        return std::nullopt;

    const uint32_t data_delta = address - ite->first;
    uint32_t start_sourced_line = static_cast<uint32_t>((data_delta / stride_bytes) * state.res_multiplier);
    uint32_t start_x = static_cast<uint32_t>((data_delta % stride_bytes) / bytes_per_pixel_requested * state.res_multiplier);

    if (static_cast<uint16_t>(start_sourced_line + height) > info.height)
        LOG_WARN_ONCE("Trying to use texture partially in the surface cache");

    // We should be able to use this texture, so set it as mru
    color_surface_queue.set_as_mru(&info);

    MetalContextImpl *context = static_cast<MetalContext *>(state.context)->impl.get();
    const bool is_same_image = context->current_color_base == info.texture;

    const bool format_matches = format == info.pixel_format || (is_srgb_format(format) && info.pixel_format == MTLPixelFormatRGBA8Unorm);
    const MTLPixelFormat view_format = format_matches ? format : info.pixel_format;

    auto get_alternate_view = [&]() -> id<MTLTexture> {
        if (swizzle_equal(swizzle, info.swizzle) && view_format == info.pixel_format)
            // we can use the same texture
            return info.texture;

        if (!info.alternate_view || info.alternate_view.pixelFormat != view_format || !swizzle_equal(info.alternate_view.swizzle, color_to_texture_swizzle(info.swizzle, swizzle))) {
            const MTLTextureSwizzleChannels resulting_mapping = color_to_texture_swizzle(info.swizzle, swizzle);
            info.alternate_view = create_texture_view(info.texture, view_format, resulting_mapping);
        }
        return info.alternate_view;
    };

    if (state.features.use_texture_viewport && base_format == info.format) {
        // use a texture viewport
        *texture_viewport = {
            .ratio = {
                original_width / static_cast<float>(info.original_width),
                original_height / static_cast<float>(info.original_height) },
            .offset = { start_x / static_cast<float>(info.width), start_sourced_line / static_cast<float>(info.height) }
        };

        return TextureLookupResult{ get_alternate_view(), view_format };
    }

    if (is_same_image || (start_sourced_line != 0) || (start_x != 0) || (info.width != width) || (info.height != height) || (info.format != base_format)) {
        const uint64_t scene_timestamp = context->scene_timestamp;

        std::vector<CastedTexture> &casted_vec = info.casted_textures;

        CastedTexture *casted = nullptr;

        // Look in cast cache and grab one
        for (size_t i = 0; i < casted_vec.size(); i++) {
            if ((casted_vec[i].cropped_height == height) && (casted_vec[i].cropped_width == width) && (casted_vec[i].cropped_y == start_sourced_line) && (casted_vec[i].cropped_x == start_x) && (casted_vec[i].format == base_format)) {
                casted = &casted_vec[i];

                if (casted->scene_timestamp == scene_timestamp) {
                    // already copied for this scene, don't do it again
                    return TextureLookupResult{ casted->texture, casted->texture.pixelFormat };
                }

                break;
            }
        }

        // use the prerender cmd as we can't copy a texture during a render pass
        id<MTLBlitCommandEncoder> blit = context->blit();

        if (casted == nullptr) {
            // Try to crop + cast
            casted_vec.resize(casted_vec.size() + 1);
            casted = &casted_vec[casted_vec.size() - 1];
            casted->cropped_x = start_x;
            casted->cropped_y = start_sourced_line;
            casted->cropped_width = width;
            casted->cropped_height = height;
            casted->format = base_format;

            // find the swizzle we need to apply
            const uint32_t components_in_store = component_count(info.pixel_format);
            const uint32_t components_requested = component_count(format);
            MTLTextureSwizzleChannels resulting_swizzle;
            // Only take into consideration the current swizzle when it makes sense
            if (bytes_per_pixel_requested == bytes_per_pixel_in_store && components_in_store == components_requested)
                resulting_swizzle = color_to_texture_swizzle(info.swizzle, swizzle);
            else
                resulting_swizzle = swizzle;

            const MTLPixelFormat casted_format = is_srgb_format(format) ? MTLPixelFormatRGBA8Unorm : format;
            id<MTLTexture> base_texture = create_texture(dev.device, casted_format, width, height, MTLTextureUsageShaderRead | MTLTextureUsagePixelFormatView, 1, false, 1, MTLStorageModePrivate, "casted surface");
            if (!base_texture) {
                casted_vec.pop_back();
                return std::nullopt;
            }
            // the copy destination must not have a swizzle, so sample through a view
            casted->texture = create_texture_view(base_texture, format, resulting_swizzle);
            casted->transition_buffer = nil;
            casted->base_texture = base_texture;
        }

        casted->scene_timestamp = scene_timestamp;

        if (bytes_per_pixel_requested == bytes_per_pixel_in_store) {
            const uint32_t copy_width = std::min<uint32_t>(width, info.width - std::min<uint32_t>(start_x, info.width));
            const uint32_t copy_height = std::min<uint32_t>(height, info.height - std::min<uint32_t>(start_sourced_line, info.height));
            if (copy_width > 0 && copy_height > 0) {
                // a texture copy between formats of the same size needs a view with the same format
                id<MTLTexture> source = info.texture;
                if (info.pixel_format != casted->base_texture.pixelFormat)
                    source = create_texture_view(info.texture, casted->base_texture.pixelFormat, identity_swizzle);
                [blit copyFromTexture:source
                          sourceSlice:0
                          sourceLevel:0
                         sourceOrigin:MTLOriginMake(start_x, start_sourced_line, 0)
                           sourceSize:MTLSizeMake(copy_width, copy_height, 1)
                            toTexture:casted->base_texture
                     destinationSlice:0
                     destinationLevel:0
                    destinationOrigin:MTLOriginMake(0, 0, 0)];
            }
        } else {
            LOG_INFO_ONCE("Game is doing typeless copies");
            // We must use a transition buffer
            const uint32_t src_pixel_stride = static_cast<uint32_t>((info.stride_bytes / bytes_per_pixel_in_store) * state.res_multiplier);
            const uint32_t src_bytes_per_row = src_pixel_stride * bytes_per_pixel_in_store;
            const uint32_t copy_height = std::min<uint32_t>(height, info.height - std::min<uint32_t>(start_sourced_line, info.height));
            const size_t buffer_size = static_cast<size_t>(src_bytes_per_row) * align(std::max(height, copy_height), 4) + start_x * bytes_per_pixel_requested + 256;
            if (!casted->transition_buffer || casted->transition_buffer.length < buffer_size) {
                casted->transition_buffer = [dev.device newBufferWithLength:buffer_size options:MTLResourceStorageModePrivate];
            }

            if (copy_height > 0) {
                // copy the texture to the buffer
                [blit copyFromTexture:info.texture
                             sourceSlice:0
                             sourceLevel:0
                            sourceOrigin:MTLOriginMake(0, start_sourced_line, 0)
                              sourceSize:MTLSizeMake(info.width, copy_height, 1)
                                toBuffer:casted->transition_buffer
                       destinationOffset:0
                  destinationBytesPerRow:src_bytes_per_row
                destinationBytesPerImage:static_cast<NSUInteger>(src_bytes_per_row) * copy_height];

                // then the buffer to the texture
                const uint32_t dst_pixel_stride = static_cast<uint32_t>((stride_bytes / bytes_per_pixel_requested) * state.res_multiplier);
                const uint32_t dst_bytes_per_row = dst_pixel_stride * bytes_per_pixel_requested;
                const uint32_t dst_height = std::min<uint32_t>(height, (static_cast<uint32_t>(src_bytes_per_row) * copy_height) / std::max(dst_bytes_per_row, 1u));
                if (dst_height > 0) {
                    [blit copyFromBuffer:casted->transition_buffer
                               sourceOffset:align(start_x * bytes_per_pixel_requested, 4)
                          sourceBytesPerRow:dst_bytes_per_row
                        sourceBytesPerImage:static_cast<NSUInteger>(dst_bytes_per_row) * dst_height
                                 sourceSize:MTLSizeMake(width, dst_height, 1)
                                  toTexture:casted->base_texture
                           destinationSlice:0
                           destinationLevel:0
                          destinationOrigin:MTLOriginMake(0, 0, 0)];
                }
            }
        }

        return TextureLookupResult{ casted->texture, casted->texture.pixelFormat };
    } else {
        return TextureLookupResult{ get_alternate_view(), view_format };
    }
}

SurfaceRetrieveResult SurfaceCache::retrieve_depth_stencil_for_framebuffer(SceGxmDepthStencilSurface *depth_stencil, const uint32_t width, const uint32_t height) {
    MetalDevice &dev = *state.device;

    // when writing we use the render target size which is already upscaled
    int32_t memory_width = static_cast<int32_t>(width / state.res_multiplier);
    int32_t memory_height = static_cast<int32_t>(height / state.res_multiplier);

    const SurfaceTiling tiling = (depth_stencil->get_type() == SCE_GXM_DEPTH_STENCIL_SURFACE_LINEAR) ? SurfaceTiling::Linear : SurfaceTiling::Tiled;

    // check if MSAA is used, the depth buffer is never downscaled
    if (target->multisample_mode != SCE_GXM_MULTISAMPLE_NONE)
        memory_height *= 2;
    if (target->multisample_mode == SCE_GXM_MULTISAMPLE_4X)
        memory_width *= 2;

    const bool is_stencil_only = depth_stencil->depth_data.address() == 0;
    DepthStencilSurfaceCacheInfo *cached_info = nullptr;

    if (!is_stencil_only) {
        auto it = depth_address_lookup.find(depth_stencil->depth_data.address());
        if (it != depth_address_lookup.end())
            cached_info = it->second;
    } else {
        auto it = stencil_address_lookup.find(depth_stencil->stencil_data.address());
        if (it != stencil_address_lookup.end())
            cached_info = it->second;
    }

    if (cached_info != nullptr) {
        // this the most recently used depth-stencil surface
        ds_surface_queue.set_as_mru(cached_info);

        const bool need_remake = cached_info->texture.width < width
            || cached_info->texture.height < height
            || cached_info->stride_samples != depth_stencil->get_stride()
            || cached_info->tiling != tiling;

        if (!need_remake)
            return { cached_info->texture, cached_info->texture, false };
    } else {
        // retrieve a new depth stencil
        cached_info = ds_surface_queue.get_lru();
    }

    // erase it if it was used previously
    if (cached_info->surface.depth_data)
        depth_address_lookup.erase(cached_info->surface.depth_data.address());
    if (cached_info->surface.stencil_data)
        stencil_address_lookup.erase(cached_info->surface.stencil_data.address());
    if (cached_info->texture)
        destroy_surface(*cached_info);

    // update the lookup info
    ds_surface_queue.set_as_mru(cached_info);
    if (depth_stencil->depth_data)
        depth_address_lookup[depth_stencil->depth_data.address()] = cached_info;
    if (depth_stencil->stencil_data)
        stencil_address_lookup[depth_stencil->stencil_data.address()] = cached_info;

    cached_info->surface = *depth_stencil;
    cached_info->memory_width = memory_width;
    cached_info->memory_height = memory_height;
    cached_info->multisample_mode = target->multisample_mode;
    cached_info->stride_samples = depth_stencil->get_stride();
    cached_info->tiling = tiling;

    uint32_t bytes_per_sample;
    switch (depth_stencil->get_format()) {
    case SCE_GXM_DEPTH_STENCIL_FORMAT_S8:
        bytes_per_sample = 1;
        break;
    case SCE_GXM_DEPTH_STENCIL_FORMAT_D16:
        bytes_per_sample = 2;
        break;
    default:
        bytes_per_sample = 4;
        break;
    }
    cached_info->total_bytes = bytes_per_sample * depth_stencil->get_stride() * memory_height;

    cached_info->texture = create_texture(dev.device, depth_stencil_format, width, height,
        MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead | MTLTextureUsagePixelFormatView, 1, false, 1, MTLStorageModePrivate, "depth-stencil surface");
    cached_info->stencil_view = nil;

    // the texture content is cleared by the next render pass
    return { cached_info->texture, cached_info->texture, true };
}

std::optional<TextureLookupResult> SurfaceCache::retrieve_depth_stencil_as_texture(const SceGxmTexture &texture, TextureViewport *texture_viewport) {
    MetalDevice &dev = *state.device;
    const SceGxmTextureBaseFormat base_format = gxm::get_base_format(gxm::get_format(texture));
    bool can_be_depth = false;
    bool can_be_stencil = false;

    uint32_t bytes_per_sample = 4;
    switch (base_format) {
        // 8bit stencil
    case SCE_GXM_TEXTURE_BASE_FORMAT_U8:
    case SCE_GXM_TEXTURE_BASE_FORMAT_S8:
        bytes_per_sample = 1;
        can_be_stencil = true;
        break;
    case SCE_GXM_TEXTURE_BASE_FORMAT_U16:
        bytes_per_sample = 2;
        [[fallthrough]];
    case SCE_GXM_TEXTURE_BASE_FORMAT_X8U24:
    case SCE_GXM_TEXTURE_BASE_FORMAT_F32:
    case SCE_GXM_TEXTURE_BASE_FORMAT_F32M:
        can_be_depth = true;
        break;
    default:
        break;
    }
    const int32_t memory_width = gxm::get_width(texture);
    const int32_t memory_height = gxm::get_height(texture);

    SurfaceTiling tiling;
    uint32_t stride_samples;

    switch (texture.texture_type()) {
    case SCE_GXM_TEXTURE_LINEAR:
        tiling = SurfaceTiling::Linear;
        stride_samples = align(memory_width, 8);
        break;
    case SCE_GXM_TEXTURE_LINEAR_STRIDED:
        tiling = SurfaceTiling::Linear;
        stride_samples = (gxm::get_stride_in_bytes(texture) * 8) / gxm::bits_per_pixel(base_format);
        break;
    case SCE_GXM_TEXTURE_TILED:
        tiling = SurfaceTiling::Tiled;
        stride_samples = align(memory_width, 32);
        break;
    default:
        // a depth/stencil is never swizzled
        return std::nullopt;
    }

    if (stride_samples % 32 != 0)
        // a depth/stencil always has a stride which is a multiple of the tile size
        return std::nullopt;

    // take upscaling into account
    uint32_t width = static_cast<uint32_t>(memory_width * state.res_multiplier);
    uint32_t height = static_cast<uint32_t>(memory_height * state.res_multiplier);
    const uint32_t total_bytes = bytes_per_sample * stride_samples * memory_height;

    const uint32_t address = texture.data_addr << 2;
    uint32_t surface_address = 0;
    DepthStencilSurfaceCacheInfo *found_info = nullptr;

    if (can_be_depth) {
        auto it = depth_address_lookup.upper_bound(address);
        if (it != depth_address_lookup.begin()) {
            --it;

            // the texture must be contained entirely in the depth surface
            if (address + total_bytes <= it->first + it->second->total_bytes) {
                surface_address = it->first;
                found_info = it->second;
            }
        }
    }
    if (!found_info && can_be_stencil) {
        auto it = stencil_address_lookup.upper_bound(address);
        if (it != stencil_address_lookup.begin()) {
            --it;

            // note: we don't support sampling the stencil from a D24S8 depth-stencil
            // so we can assume any stencil uses only 1 byte per sample
            const uint32_t surface_bytes = it->second->stride_samples * it->second->memory_height * 1;

            if (address + total_bytes <= it->first + surface_bytes) {
                surface_address = it->first;
                found_info = it->second;
            }
        }
    }

    if (found_info == nullptr)
        return std::nullopt;

    DepthStencilSurfaceCacheInfo &cached_info = *found_info;
    if (tiling != cached_info.tiling || stride_samples != cached_info.stride_samples)
        return std::nullopt;

    // we sample from it, set the surface as most recently used
    ds_surface_queue.set_as_mru(found_info);

    // take MSAA into account
    if (cached_info.multisample_mode != SCE_GXM_MULTISAMPLE_NONE)
        height /= 2;
    if (cached_info.multisample_mode == SCE_GXM_MULTISAMPLE_4X)
        width /= 2;

    const bool is_stencil = can_be_stencil;
    if (is_stencil)
        LOG_WARN_ONCE("Sampling a stencil surface is not fully supported by the Metal renderer");

    const uint32_t delta_samples = (address - surface_address) / bytes_per_sample;
    const uint32_t delta_col_samples = delta_samples % stride_samples;
    const uint32_t delta_row_samples = delta_samples / stride_samples;

    MetalContextImpl *context = static_cast<MetalContext *>(state.context)->impl.get();
    const bool reading_ds_attachment = cached_info.texture == context->current_ds_texture;
    const bool same_dimension = memory_width == cached_info.memory_width
        && memory_height == cached_info.memory_height
        && delta_col_samples == 0
        && delta_row_samples == 0;

    if (!reading_ds_attachment && (state.features.use_texture_viewport || same_dimension)) {
        // we can just sample from the surface itself
        id<MTLTexture> view = cached_info.texture;
        if (is_stencil) {
            if (!cached_info.stencil_view)
                cached_info.stencil_view = create_texture_view(cached_info.texture, stencil_view_format, identity_swizzle);
            view = cached_info.stencil_view;
        }

        const float inv_surface_width = 1 / static_cast<float>(cached_info.memory_width);
        const float inv_surface_height = 1 / static_cast<float>(cached_info.memory_height);
        if (state.features.use_texture_viewport) {
            texture_viewport->offset = {
                delta_col_samples * inv_surface_width,
                delta_row_samples * inv_surface_height
            };
            texture_viewport->ratio = {
                memory_width * inv_surface_width,
                memory_height * inv_surface_height
            };
        }

        return TextureLookupResult{ view, view.pixelFormat };
    }

    const uint64_t scene_timestamp = context->scene_timestamp;

    int read_surface_idx = -1;
    for (int i = 0; i < static_cast<int>(cached_info.read_surfaces.size()); i++) {
        auto &read_surface = cached_info.read_surfaces[i];
        if (read_surface.width == width
            && read_surface.height == height
            && read_surface.delta_row == delta_row_samples
            && read_surface.delta_col == delta_col_samples) {
            read_surface_idx = i;
            break;
        }
    }

    if (read_surface_idx == -1) {
        // no compatible read surface found
        DepthSurfaceView read_only;
        read_only.width = width;
        read_only.height = height;
        read_only.delta_col = delta_col_samples;
        read_only.delta_row = delta_row_samples;
        read_only.scene_timestamp = 0;
        read_only.depth_view = create_texture(dev.device, depth_stencil_format, width, height,
            MTLTextureUsageShaderRead | MTLTextureUsagePixelFormatView, 1, false, 1, MTLStorageModePrivate, "depth read surface");
        if (!read_only.depth_view)
            return std::nullopt;

        read_surface_idx = static_cast<int>(cached_info.read_surfaces.size());
        cached_info.read_surfaces.emplace_back(std::move(read_only));
    }

    DepthSurfaceView &read_only = cached_info.read_surfaces[read_surface_idx];
    id<MTLTexture> view = read_only.depth_view;
    if (is_stencil) {
        if (!read_only.stencil_view)
            read_only.stencil_view = create_texture_view(read_only.depth_view, stencil_view_format, identity_swizzle);
        view = read_only.stencil_view;
    }

    // copy the depth stencil only once per scene
    if (read_only.scene_timestamp == scene_timestamp)
        return TextureLookupResult{ view, view.pixelFormat };

    read_only.scene_timestamp = scene_timestamp;

    const uint32_t delta_row = static_cast<uint32_t>(delta_row_samples * state.res_multiplier);
    const uint32_t delta_col = static_cast<uint32_t>(delta_col_samples * state.res_multiplier);
    const uint32_t copy_width = std::min<uint32_t>(width, static_cast<uint32_t>(cached_info.texture.width) - std::min<uint32_t>(delta_col, cached_info.texture.width));
    const uint32_t copy_height = std::min<uint32_t>(height, static_cast<uint32_t>(cached_info.texture.height) - std::min<uint32_t>(delta_row, cached_info.texture.height));

    if (copy_width > 0 && copy_height > 0) {
        id<MTLBlitCommandEncoder> blit = context->blit();
        [blit copyFromTexture:cached_info.texture
                  sourceSlice:0
                  sourceLevel:0
                 sourceOrigin:MTLOriginMake(delta_col, delta_row, 0)
                   sourceSize:MTLSizeMake(copy_width, copy_height, 1)
                    toTexture:read_only.depth_view
             destinationSlice:0
             destinationLevel:0
            destinationOrigin:MTLOriginMake(0, 0, 0)];
    }

    return TextureLookupResult{ view, view.pixelFormat };
}

id<MTLTexture> SurfaceCache::sourcing_color_surface_for_presentation(Ptr<const void> address, uint32_t pitch, Viewport &viewport) {
    // get closest surface with an address below address
    auto ite = color_address_lookup.upper_bound(address.address());
    if (ite == color_address_lookup.begin()) {
        return nil;
    }
    --ite;

    ColorSurfaceCacheInfo &info = *ite->second;
    if (info.data.address() + info.total_bytes <= address.address())
        // they do not overlap
        return nil;

    if (info.stride_bytes == pitch * 4) {
        // In assumption the format is RGBA8
        const size_t data_delta = address.address() - ite->first;
        uint32_t limited_height = viewport.height;
        if ((data_delta % (pitch * 4)) == 0) {
            const uint32_t start_sourced_line = static_cast<uint32_t>((data_delta / (pitch * 4)) * state.res_multiplier);
            if ((start_sourced_line + viewport.height) > info.height) {
                // Sometimes the surface is just missing a little bit of lines
                if (start_sourced_line < info.height) {
                    // Just limit the height and display it
                    limited_height = info.height - start_sourced_line;
                } else {
                    LOG_ERROR("Trying to present non-existent segment in cached color surface!");
                    return nil;
                }
            }

            // Compute position in texture
            viewport.offset_x = 0;
            viewport.offset_y = start_sourced_line;
            viewport.width = std::min(viewport.width, static_cast<uint32_t>(info.width));
            viewport.height = limited_height;
            viewport.texture_width = info.width;
            viewport.texture_height = info.height;

            if (swizzle_is_identity(info.swizzle) && info.pixel_format == MTLPixelFormatRGBA8Unorm)
                return info.texture;

            if (info.pixel_format != MTLPixelFormatRGBA8Unorm)
                return nil;

            // create a view with the right swizzle and without gamma correction
            const MTLTextureSwizzleChannels present_swizzle = color_to_texture_swizzle(info.swizzle, identity_swizzle);
            if (!info.alternate_view || info.alternate_view.pixelFormat != MTLPixelFormatRGBA8Unorm || !swizzle_equal(info.alternate_view.swizzle, present_swizzle))
                info.alternate_view = create_texture_view(info.texture, MTLPixelFormatRGBA8Unorm, present_swizzle);

            return info.alternate_view;
        }
    }

    return nil;
}

std::vector<uint32_t> SurfaceCache::dump_frame(Ptr<const void> address, uint32_t width, uint32_t height, uint32_t pitch) {
    MetalDevice &dev = *state.device;

    // get closest surface with an address below address
    auto ite = color_address_lookup.upper_bound(address.address());
    if (ite == color_address_lookup.begin()) {
        return {};
    }
    --ite;

    const ColorSurfaceCacheInfo &info = *ite->second;

    const uint32_t data_delta = address.address() - ite->first;
    const uint32_t pitch_byte = pitch * 4;
    if (info.stride_bytes != pitch_byte || data_delta % pitch_byte != 0 || info.pixel_format != MTLPixelFormatRGBA8Unorm)
        return {};

    const uint32_t line_delta = static_cast<uint32_t>((data_delta / pitch_byte) * state.res_multiplier);
    if (line_delta >= info.height)
        return {};

    const uint32_t real_height = std::min(height, info.height - line_delta);
    const uint32_t real_width = std::min<uint32_t>(width, info.width);

    std::vector<uint32_t> frame(static_cast<size_t>(width) * height, 0);

    id<MTLBuffer> temp_buffer = create_shared_buffer(dev.device, static_cast<size_t>(width) * height * 4, "frame dump");
    id<MTLCommandBuffer> cmd = [dev.queue commandBuffer];
    id<MTLBlitCommandEncoder> blit = [cmd blitCommandEncoder];
    [blit copyFromTexture:info.texture
              sourceSlice:0
              sourceLevel:0
             sourceOrigin:MTLOriginMake(0, line_delta, 0)
               sourceSize:MTLSizeMake(real_width, real_height, 1)
                 toBuffer:temp_buffer
        destinationOffset:0
   destinationBytesPerRow:width * 4
 destinationBytesPerImage:static_cast<NSUInteger>(width) * height * 4];
    [blit endEncoding];
    [cmd commit];
    [cmd waitUntilCompleted];

    memcpy(frame.data(), temp_buffer.contents, frame.size() * 4);

    return frame;
}

} // namespace renderer::metal
