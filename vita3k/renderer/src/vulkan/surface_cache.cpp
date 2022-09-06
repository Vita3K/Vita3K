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

#include <renderer/vulkan/surface_cache.h>

#include <gxm/functions.h>
#include <renderer/vulkan/gxm_to_vulkan.h>
#include <renderer/vulkan/state.h>
#include <renderer/vulkan/types.h>
#include <vkutil/vkutil.h>

#include <vulkan/vulkan_format_traits.hpp>

#include <util/align.h>
#include <util/log.h>
#include <util/vector_utils.h>

extern "C" {
#include <libswscale/swscale.h>
}

static bool format_support_surface_sync(SceGxmColorBaseFormat format) {
    // we use rgba16 to emulate this format, don't even try to convert it back for now
    return format != SCE_GXM_COLOR_BASE_FORMAT_U2F10F10F10;
}

static bool format_support_swizzle(SceGxmColorBaseFormat format) {
    // do we support something more than the identity swizzle
    // for now we do not support any texture whose component size
    // are all not the same or not a multiple of a byte
    return format != SCE_GXM_COLOR_BASE_FORMAT_U2F10F10F10
        && format != SCE_GXM_COLOR_BASE_FORMAT_U2U10U10U10
        && format != SCE_GXM_COLOR_BASE_FORMAT_U4U4U4U4
        && format != SCE_GXM_COLOR_BASE_FORMAT_U1U5U5U5
        && format != SCE_GXM_COLOR_BASE_FORMAT_SE5M9M9M9
        && format != SCE_GXM_COLOR_BASE_FORMAT_F11F11F10
        && format != SCE_GXM_COLOR_BASE_FORMAT_U5U6U5;
}

static bool format_need_additional_memory(SceGxmColorBaseFormat format) {
    // we are using 4-component surfaces to emulate them
    // so we can't simply use the allocated memory for them
    return format == SCE_GXM_COLOR_BASE_FORMAT_U8U8U8;
}

namespace renderer::vulkan {

ColorSurfaceCacheInfo::~ColorSurfaceCacheInfo() {
    sws_freeContext(sws_context);
}

void VKSurfaceCache::destroy_framebuffers(vk::ImageView view) {
    vkutil::DestroyQueue &destroy_queue = state.frame().destroy_queue;
    for (auto it = framebuffer_array.begin(); it != framebuffer_array.end();) {
        // if the color of depth-stencil match the one of the render_target, this won't be used anymore
        if (it->first.first == view || it->first.second == view) {
            destroy_queue.add(it->second.standard);
            destroy_queue.add(it->second.shader_interlock);
            it = framebuffer_array.erase(it);
        } else {
            it = std::next(it);
        }
    }
}

void VKSurfaceCache::destroy_surface(ColorSurfaceCacheInfo &info) {
    vkutil::DestroyQueue &destroy_queue = state.frame().destroy_queue;

    // don't forget to destroy in the right order
    for (auto &casted : info.casted_textures) {
        destroy_queue.add_buffer(casted.transition_buffer);
        destroy_queue.add_image(casted.texture);
    }
    info.casted_textures.clear();

    destroy_queue.add(info.alternate_view);

    destroy_framebuffers(info.texture.view);
    destroy_queue.add_image(info.texture);
}

void VKSurfaceCache::destroy_surface(DepthStencilSurfaceCacheInfo &info) {
    vkutil::DestroyQueue &destroy_queue = state.frame().destroy_queue;

    for (auto &read_only : info.read_surfaces) {
        destroy_queue.add_image(read_only.stencil_view);
        destroy_queue.add_image(read_only.depth_view);
    }
    info.read_surfaces.clear();

    destroy_queue.add(info.depth_view);
    destroy_queue.add(info.stencil_view);

    destroy_framebuffers(info.texture.view);
    destroy_queue.add_image(info.texture);
}

VKSurfaceCache::VKSurfaceCache(VKState &state)
    : state(state) {
    color_surface_queue.init(max_surfaces_allowed);
    ds_surface_queue.init(max_surfaces_allowed);
}

SurfaceRetrieveResult VKSurfaceCache::retrieve_color_surface_for_framebuffer(MemState &mem, SceGxmColorSurface *color) {
    // Create the key to access the cache struct
    const uint32_t address = color->data.address();

    const uint32_t original_width = color->width;
    const uint32_t original_height = color->height;

    uint32_t width = static_cast<uint32_t>(original_width * state.res_multiplier);
    uint32_t height = static_cast<uint32_t>(original_height * state.res_multiplier);

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
    vk::Format vk_format = color::translate_format(base_format);

    SurfaceTiling tiling;
    if (color->surfaceType == SCE_GXM_COLOR_SURFACE_LINEAR)
        tiling = SurfaceTiling::Linear;
    else if (color->surfaceType == SCE_GXM_COLOR_SURFACE_SWIZZLED)
        tiling = SurfaceTiling::Swizzled;
    else
        tiling = SurfaceTiling::Tiled;

    const bool is_srgb = color->gamma != 0;
    if (is_srgb) {
        if (vk_format == vk::Format::eR8G8B8A8Unorm) {
            vk_format = vk::Format::eR8G8B8A8Srgb;
        } else {
            LOG_WARN_ONCE("Trying to use gamma correction with non-compatible format {}", vk::to_string(vk_format));
        }
    }

    uint32_t bytes_per_stride = color->strideInPixels * gxm::bits_per_pixel(base_format) / 8;
    uint32_t total_surface_size = bytes_per_stride * original_height;

    VKContext *context = reinterpret_cast<VKContext *>(state.context);

    if (overlap) {
        ColorSurfaceCacheInfo &info = *ite->second;

        // There are four situations I think of:
        // 1. Different base address, lookup for write, in this case, if the cached surface range contains the given address, then
        // probably this cached surface has already been freed GPU-wise. So erase.
        // 2. Same base address, but width and height change to be larger, or format change if write. Remake a new one for both read and write situation.
        // 3. Out of cache range. In write case, create a new one, in read case, lul
        // 4. Read situation with smaller width and height, probably need to extract the needed region out.
        // 5. the surface is a gbuffer and we are currently trying to read the 2nd component, in this case key == ite->first + 4
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
            last_written_surface = &info;

            // if this surface has not been rendered to for the last 60 frames, consider it is not safe not to render all shaders to it
            constexpr uint64_t big_delay_between_frames = 60;
            state.pipeline_cache.can_use_deferred_compilation = context->frame_timestamp - info.last_frame_rendered < big_delay_between_frames;
            info.last_frame_rendered = context->frame_timestamp;

            if (vk_format == info.texture.format) {
                return { info.texture.view, &info.texture };
            } else {
                // using both srgb/linear
                if (!info.alternate_view) {
                    vk::ImageViewCreateInfo view_info{
                        .image = info.texture.image,
                        .viewType = vk::ImageViewType::e2D,
                        .format = vk_format,
                        .components = vkutil::default_comp_mapping,
                        .subresourceRange = vkutil::color_subresource_range
                    };
                    info.alternate_view = state.device.createImageView(view_info);
                }

                return { info.alternate_view, &info.texture };
            }
        }
    }

    // get the least recently used (probably unused) color surface
    ColorSurfaceCacheInfo &info_added = *color_surface_queue.get_lru();
    if (info_added.texture.image)
        // deferred destruction of the existing surface
        destroy_surface(info_added);
    if (info_added.data)
        color_address_lookup.erase(info_added.data.address());

    color_surface_queue.set_as_mru(&info_added);
    info_added.last_frame_rendered = context->frame_timestamp;

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

    vkutil::Image &image = info_added.texture;
    image.width = width;
    image.height = height;
    image.format = vk_format;
    image.layout = vkutil::ImageLayout::Undefined;

    // we might have to create a non-srgb/linear view later if this surface is used for presentation
    const bool need_mutable = (vk_format == vk::Format::eR8G8B8A8Unorm || vk_format == vk::Format::eR8G8B8A8Srgb);
    const vk::ImageCreateFlags image_create_flags = need_mutable ? vk::ImageCreateFlagBits::eMutableFormat : vk::ImageCreateFlags();
    const void *image_info_pNext = nullptr;
    if (support_image_format_specifier && need_mutable) {
        static const vk::Format view_formats[] = { vk::Format::eR8G8B8A8Unorm, vk::Format::eR8G8B8A8Srgb };
        static const vk::ImageFormatListCreateInfoKHR image_info_formats{
            .viewFormatCount = 2,
            .pViewFormats = view_formats
        };
        image_info_pNext = &image_info_formats;
    }

    vk::ImageUsageFlags surface_usages = vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eInputAttachment;
    if (state.features.support_shader_interlock)
        surface_usages |= vk::ImageUsageFlagBits::eStorage;
    image.init_image(surface_usages, vkutil::default_comp_mapping, image_create_flags, image_info_pNext);

    // do it in the prerender if we read from this texture in the same scene (although this would be useless)
    vk::CommandBuffer cmd_buffer = context->prerender_cmd;
    // must do a first transition to draw the placeholder color
    image.transition_to(cmd_buffer, vkutil::ImageLayout::TransferDst);

    vk::ClearColorValue clear_color{ std::array<float, 4>({ 0.0f, 0.0f, 0.0f, 0.0f }) };
    cmd_buffer.clearColorImage(image.image, vk::ImageLayout::eTransferDstOptimal, clear_color, vkutil::color_subresource_range);
    image.transition_to(cmd_buffer, vkutil::ImageLayout::ColorAttachmentReadWrite);

    last_written_surface = &info_added;
    info_added.need_surface_sync.reset();
    info_added.need_surface_sync = std::make_shared<bool>();
    *info_added.need_surface_sync = false;

    // we only support surface sync of linear surfaces for now
    if (!can_mprotect_mapped_memory) {
        // perform surface sync on everything
        // it is slow but well... we can't mprotect the buffer
        *info_added.need_surface_sync = color->surfaceType == SCE_GXM_COLOR_SURFACE_LINEAR;
    } else if (color->surfaceType == SCE_GXM_COLOR_SURFACE_LINEAR && format_support_surface_sync(base_format)) {
        uint32_t addr_start = align(info_added.data.address(), KiB(4));
        uint32_t addr_end = align_down(info_added.data.address() + info_added.total_bytes, KiB(4));
        if (addr_start >= addr_end) {
            // we still need to protect something, even if it's not completely accurate
            addr_start = align_down(info_added.data.address(), KiB(4));
            addr_end = align(info_added.data.address() + info_added.total_bytes, KiB(4));
        }
        add_protect(mem, addr_start, addr_end - addr_start, MemPerm::None, [need_sync = info_added.need_surface_sync](Address addr, bool) {
            *need_sync = true;
            return true;
        });
    }

    // it's not impossible that this surface will be rendered once and only used after, so do not skip any shader on it
    state.pipeline_cache.can_use_deferred_compilation = false;

    return { info_added.texture.view, &info_added.texture };
}

std::optional<TextureLookupResult> VKSurfaceCache::retrieve_color_surface_as_texture(const SceGxmTexture &texture, const SceGxmColorBaseFormat base_format, TextureViewport *texture_viewport) {
    // Create the key to access the cache struct
    const uint32_t address = (texture.data_addr << 2);

    const uint32_t original_width = gxm::get_width(texture);
    const uint32_t original_height = gxm::get_height(texture);

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

    if (!overlap)
        return std::nullopt;

    const vk::ComponentMapping swizzle = texture::translate_swizzle(gxm::get_format(texture));
    vk::Format vk_format = color::translate_format(base_format);

    const bool is_srgb = texture.gamma_mode != 0;
    if (is_srgb) {
        if (vk_format == vk::Format::eR8G8B8A8Unorm) {
            vk_format = vk::Format::eR8G8B8A8Srgb;
        } else {
            LOG_WARN_ONCE("Trying to use gamma correction with non-compatible format {}", vk::to_string(vk_format));
        }
    }

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
    uint32_t total_surface_size = stride_bytes * original_height;

    ColorSurfaceCacheInfo &info = *ite->second;

    if ((base_format == SCE_GXM_COLOR_BASE_FORMAT_U8U8U8 || info.format == SCE_GXM_COLOR_BASE_FORMAT_U8U8U8)
        && base_format != info.format)
        // don't even try to match u8u8u8 with something else
        return std::nullopt;

    if (tiling != info.tiling || info.stride_bytes != stride_bytes)
        // if the tiling is different, also don't try to match them
        // about the strides, I've yet to see a case where the byte stride is different
        return std::nullopt;

    // Check if we can use this surface
    bool addr_in_range_of_cache = ((address + total_surface_size) <= (ite->first + info.total_bytes + 4));

    if (ite->first != address && !addr_in_range_of_cache)
        // persona 4 sample from the top of a texture while the bottom wasn't rendered to, the fact that both the surface and
        // the texture start at the same location should be enough
        return std::nullopt;

    uint32_t bytes_per_pixel_requested = gxm::bits_per_pixel(base_format) / 8;
    uint32_t bytes_per_pixel_in_store = gxm::bits_per_pixel(info.format) / 8;

    if (std::max(bytes_per_pixel_requested, bytes_per_pixel_in_store) % std::min(bytes_per_pixel_requested, bytes_per_pixel_in_store) != 0)
        return std::nullopt;

    // TODO: this is true only for linear textures (and also kind of for tiled textures) (and in this case start_x = 0),
    // for swizzled textures this is different
    const uint32_t data_delta = address - ite->first;
    uint32_t start_sourced_line = static_cast<uint32_t>((data_delta / stride_bytes) * state.res_multiplier);
    uint32_t start_x = static_cast<uint32_t>((data_delta % stride_bytes) / bytes_per_pixel_requested * state.res_multiplier);

    if (static_cast<uint16_t>(start_sourced_line + height) > info.height)
        LOG_WARN_ONCE("Trying to use texture partially in the surface cache");

    // We should be able to use this texture, so set it as mru
    color_surface_queue.set_as_mru(&info);

    const vk::ImageView color_handle_view = reinterpret_cast<VKContext *>(state.context)->current_color_view;
    const bool is_same_image = (color_handle_view == info.texture.view) || (color_handle_view == info.alternate_view);

    if (state.features.use_texture_viewport && base_format == info.format) {
        // use a texture viewport
        *texture_viewport = {
            .ratio = {
                original_width / static_cast<float>(info.original_width),
                original_height / static_cast<float>(info.original_height) },
            .offset = { start_x / static_cast<float>(info.width), start_sourced_line / static_cast<float>(info.height) }
        };

        // if everything matches
        if (vk_format == info.texture.format && swizzle == info.swizzle)
            return TextureLookupResult{
                info.texture.view,
                info.texture.layout,
                info.texture.format
            };

        // use the other view with the correct swizzle / gamma correction
        if (!info.alternate_view) {
            vk::ComponentMapping resulting_mapping = vkutil::color_to_texture_swizzle(info.swizzle, swizzle);

            vk::ImageViewCreateInfo view_info{
                .image = info.texture.image,
                .viewType = vk::ImageViewType::e2D,
                .format = vk_format,
                .components = resulting_mapping,
                .subresourceRange = vkutil::color_subresource_range
            };
            info.alternate_view = state.device.createImageView(view_info);
        }

        return TextureLookupResult{
            info.alternate_view,
            info.texture.layout,
            info.texture.format
        };
    }

    if (is_same_image || (start_sourced_line != 0) || (start_x != 0) || (info.width != width) || (info.height != height) || (info.format != base_format)) {
        const uint64_t scene_timestamp = reinterpret_cast<VKContext *>(state.context)->scene_timestamp;

        std::vector<CastedTexture> &casted_vec = info.casted_textures;

        CastedTexture *casted = nullptr;

        // Look in cast cache and grab one. The cache really does not store immediate grab on now, but rather to reduce the synchronization in the pipeline (use different texture)
        for (size_t i = 0; i < casted_vec.size();) {
            if ((casted_vec[i].cropped_height == height) && (casted_vec[i].cropped_width == width) && (casted_vec[i].cropped_y == start_sourced_line) && (casted_vec[i].cropped_x == start_x) && (casted_vec[i].format == base_format)) {
                casted = &casted_vec[i];

                if (casted->scene_timestamp == scene_timestamp) {
                    // already copied for this scene, don't do it again
                    return TextureLookupResult{
                        casted->texture.view,
                        casted->texture.layout,
                        casted->texture.format
                    };
                }

                break;
            } else {
                i++;
            }
        }

        // use prerender cmd as we can't copy an image or use pipeline barriers in a render pass
        VKContext *context = reinterpret_cast<VKContext *>(state.context);
        vk::CommandBuffer cmd_buffer = context->prerender_cmd;

        if (casted == nullptr) {
            // Try to crop + cast
            casted_vec.resize(casted_vec.size() + 1);
            casted = &casted_vec[casted_vec.size() - 1];
            *casted = CastedTexture{
                .cropped_x = start_x,
                .cropped_y = start_sourced_line,
                .cropped_width = width,
                .cropped_height = height,
                .format = base_format
            };
            casted->texture.width = width;
            casted->texture.height = height;
            casted->texture.format = vk_format;

            // find the swizzle we need to apply
            const std::uint8_t components_in_store = vk::componentCount(info.texture.format);
            const std::uint8_t components_requested = vk::componentCount(vk_format);
            vk::ComponentMapping resulting_swizzle;
            // Only take into consideration the current swizzle when it makes sense
            // (Not perfect but better than doing this all the time)
            if (bytes_per_pixel_requested == bytes_per_pixel_in_store && components_in_store == components_requested)
                resulting_swizzle = vkutil::color_to_texture_swizzle(info.swizzle, swizzle);
            else
                resulting_swizzle = swizzle;

            casted->texture.init_image(vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst, resulting_swizzle);
            casted->texture.transition_to(cmd_buffer, vkutil::ImageLayout::TransferDst);
        } else {
            casted->texture.transition_to_discard(cmd_buffer, vkutil::ImageLayout::TransferDst);
        }

        casted->scene_timestamp = scene_timestamp;

        if (bytes_per_pixel_requested == bytes_per_pixel_in_store) {
            vk::ImageCopy image_copy{
                .srcSubresource = vkutil::color_subresource_layer,
                .srcOffset = { static_cast<int32_t>(start_x), static_cast<int32_t>(start_sourced_line), 0 },
                .dstSubresource = vkutil::color_subresource_layer,
                .dstOffset = { 0,
                    0,
                    0 },
                .extent = {
                    // Don't try to copy what is in the stride
                    std::min<uint32_t>(width, info.width),
                    std::min<uint32_t>(height, info.height),
                    1 }
            };
            cmd_buffer.copyImage(info.texture.image, vk::ImageLayout::eGeneral, casted->texture.image, vk::ImageLayout::eTransferDstOptimal, image_copy);
        } else {
            LOG_INFO_ONCE("Game is doing typeless copies");
            // We must use a transition buffer
            vk::DeviceSize buffer_size = stride_bytes * static_cast<size_t>(state.res_multiplier * align(height, 4)) + start_x * bytes_per_pixel_requested;
            if (!casted->transition_buffer.buffer || casted->transition_buffer.size < buffer_size) {
                // create or re-create the buffer
                state.frame().destroy_queue.add_buffer(casted->transition_buffer);
                casted->transition_buffer = vkutil::Buffer(buffer_size);
                casted->transition_buffer.init_buffer(vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eTransferSrc);
            }

            // copy the image to the buffer
            const uint32_t src_pixel_stride = static_cast<uint32_t>((info.stride_bytes / bytes_per_pixel_in_store) * state.res_multiplier);
            vk::BufferImageCopy copy_image_buffer{
                .bufferOffset = 0,
                .bufferRowLength = src_pixel_stride,
                .bufferImageHeight = height,
                .imageSubresource = vkutil::color_subresource_layer,
                .imageOffset = { 0,
                    static_cast<int32_t>(start_sourced_line),
                    0 },
                .imageExtent = { info.width, height, 1 }
            };
            cmd_buffer.copyImageToBuffer(info.texture.image, vk::ImageLayout::eGeneral, casted->transition_buffer.buffer, copy_image_buffer);

            // then the buffer to the image
            const uint32_t dst_pixel_stride = (stride_bytes / bytes_per_pixel_requested) * state.res_multiplier;
            copy_image_buffer
                .setBufferOffset(start_x * bytes_per_pixel_requested)
                .setBufferRowLength(dst_pixel_stride)
                .setImageOffset({ 0, 0, 0 })
                .setImageExtent({ width, height, 1 });
            cmd_buffer.copyBufferToImage(casted->transition_buffer.buffer, casted->texture.image, vk::ImageLayout::eTransferDstOptimal, copy_image_buffer);
        }
        casted->texture.transition_to(cmd_buffer, vkutil::ImageLayout::ColorAttachmentReadWrite);

        return TextureLookupResult{
            casted->texture.view,
            casted->texture.layout,
            casted->texture.format
        };
    } else {
        // the renderpass external dependencies should take care of the barrier
        if (swizzle == info.swizzle && vk_format == info.texture.format)
            // we can use the same texture view
            return TextureLookupResult{
                info.texture.view,
                info.texture.layout,
                info.texture.format
            };

        if (!info.alternate_view) {
            vk::ComponentMapping resulting_mapping = vkutil::color_to_texture_swizzle(info.swizzle, swizzle);

            vk::ImageViewCreateInfo view_info{
                .image = info.texture.image,
                .viewType = vk::ImageViewType::e2D,
                .format = vk_format,
                .components = resulting_mapping,
                .subresourceRange = vkutil::color_subresource_range
            };
            info.alternate_view = state.device.createImageView(view_info);
        }

        return TextureLookupResult{
            info.alternate_view,
            vkutil::ImageLayout::ColorAttachmentReadWrite,
            vk_format
        };
    }
}

SurfaceRetrieveResult VKSurfaceCache::retrieve_depth_stencil_for_framebuffer(SceGxmDepthStencilSurface *depth_stencil, const uint32_t width, const uint32_t height) {
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

        bool need_remake = cached_info->texture.width < width
            || cached_info->texture.height < height
            || cached_info->stride_samples != depth_stencil->get_stride()
            || cached_info->tiling != tiling;

        if (!need_remake)
            return {
                cached_info->texture.view,
                &cached_info->texture
            };
    } else {
        // retrieve a new depth stencil
        cached_info = ds_surface_queue.get_lru();
    }

    // erase it if it was used previously
    if (cached_info->surface.depth_data)
        depth_address_lookup.erase(cached_info->surface.depth_data.address());
    if (cached_info->surface.stencil_data)
        stencil_address_lookup.erase(cached_info->surface.stencil_data.address());
    if (cached_info->texture.image)
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

    vkutil::Image &image = cached_info->texture;

    // use prerender cmd in case we read from the depth buffer (although I really doubt this could happen)
    VKContext *context = reinterpret_cast<VKContext *>(state.context);
    vk::CommandBuffer cmd_buffer = context->prerender_cmd;

    image.width = width;
    image.height = height;
    image.format = vk::Format::eD32SfloatS8Uint;
    image.layout = vkutil::ImageLayout::Undefined;
    image.init_image(vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eSampled);

    image.transition_to(cmd_buffer, vkutil::ImageLayout::TransferDst, vkutil::ds_subresource_range);
    vk::ClearDepthStencilValue clear_value{
        .depth = 1.0,
        .stencil = 0
    };
    cmd_buffer.clearDepthStencilImage(image.image, vk::ImageLayout::eTransferDstOptimal, clear_value, vkutil::ds_subresource_range);
    image.transition_to(cmd_buffer, vkutil::ImageLayout::DepthStencilReadOnly, vkutil::ds_subresource_range);

    return {
        image.view,
        &image
    };
}

std::optional<TextureLookupResult> VKSurfaceCache::retrieve_depth_stencil_as_texture(const SceGxmTexture &texture, TextureViewport *texture_viewport) {
    SceGxmTextureBaseFormat base_format = gxm::get_base_format(gxm::get_format(texture));
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
    int32_t memory_width = gxm::get_width(texture);
    int32_t memory_height = gxm::get_height(texture);

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
    uint32_t total_bytes = bytes_per_sample * stride_samples * memory_height;

    const uint32_t address = texture.data_addr << 2;
    uint32_t surface_address = 0;
    DepthStencilSurfaceCacheInfo *found_info = nullptr;

    if (can_be_depth) {
        // get the first depth surface with an address lower or equal to address
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
        // get the first stencil surface with an address lower or equal to address
        auto it = stencil_address_lookup.upper_bound(address);
        if (it != stencil_address_lookup.begin()) {
            --it;

            // note: we don't support sampling the stencil from a D24S8 depth-stencil
            // so we can assume any stencil uses only 1 byte per sample
            uint32_t surface_bytes = it->second->stride_samples * it->second->memory_height * 1;

            // the texture must be contained entirely in the stencil surface
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

    const uint32_t delta_samples = (address - surface_address) / bytes_per_sample;
    uint32_t delta_col_samples = delta_samples % stride_samples;
    uint32_t delta_row_samples = delta_samples / stride_samples;

    vk::ImageView ds_attachment = reinterpret_cast<VKContext *>(state.context)->current_ds_view;
    const bool reading_ds_attachment = cached_info.texture.view == ds_attachment;
    const bool same_dimension = memory_width == cached_info.memory_width
        && memory_height == cached_info.memory_height
        && delta_col_samples == 0
        && delta_row_samples == 0;

    if (!reading_ds_attachment && (state.features.use_texture_viewport || same_dimension)) {
        // we can just sample from the surface itself

        // we must create a new read-only view if it is not already present
        vk::ImageView &img_view = is_stencil ? cached_info.stencil_view : cached_info.depth_view;
        if (!img_view) {
            vk::ImageSubresourceRange range = vkutil::ds_subresource_range;
            range.aspectMask = is_stencil ? vk::ImageAspectFlagBits::eStencil : vk::ImageAspectFlagBits::eDepth;
            vk::ImageViewCreateInfo view_info{
                .image = cached_info.texture.image,
                .viewType = vk::ImageViewType::e2D,
                .format = vk::Format::eD32SfloatS8Uint,
                .components = {},
                .subresourceRange = range
            };
            img_view = state.device.createImageView(view_info);
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

        return TextureLookupResult{
            img_view,
            vkutil::ImageLayout::DepthStencilReadOnly,
            vk::Format::eD32SfloatS8Uint
        };
    }

    const uint64_t scene_timestamp = reinterpret_cast<VKContext *>(state.context)->scene_timestamp;

    int read_surface_idx = -1;
    for (int i = 0; i < cached_info.read_surfaces.size(); i++) {
        auto &read_surface = cached_info.read_surfaces[i];
        if (read_surface.depth_view.width == width
            && read_surface.depth_view.height == height
            && read_surface.delta_row == delta_row_samples
            && read_surface.delta_col == delta_col_samples) {
            read_surface_idx = i;
            break;
        }
    }

    if (read_surface_idx == -1) {
        // no compatible read surface found

        DepthSurfaceView read_only{
            .depth_view = vkutil::Image(width, height, vk::Format::eD32SfloatS8Uint),
            .scene_timestamp = 0,
            .delta_col = delta_col_samples,
            .delta_row = delta_row_samples,
        };
        read_only.depth_view.init_image(vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst);
        // we want a texture view with only the depth or stencil aspect bit
        // TODO: not efficient
        state.device.destroy(read_only.depth_view.view);
        read_only.depth_view.view = nullptr;

        read_surface_idx = cached_info.read_surfaces.size();
        cached_info.read_surfaces.emplace_back(std::move(read_only));
    }

    DepthSurfaceView &read_only = cached_info.read_surfaces[read_surface_idx];
    vkutil::Image &img_view = is_stencil ? read_only.stencil_view : read_only.depth_view;

    if (!img_view.view) {
        vk::ImageSubresourceRange range = vkutil::ds_subresource_range;
        range.aspectMask = is_stencil ? vk::ImageAspectFlagBits::eStencil : vk::ImageAspectFlagBits::eDepth;
        vk::ImageViewCreateInfo view_info{
            .image = read_only.depth_view.image,
            .viewType = vk::ImageViewType::e2D,
            .format = vk::Format::eD32SfloatS8Uint,
            .components = {},
            .subresourceRange = range
        };
        img_view.view = state.device.createImageView(view_info);
        img_view.layout = vkutil::ImageLayout::SampledImage;
    }

    // copy the depth stencil only once per scene
    if (read_only.scene_timestamp == scene_timestamp)
        return TextureLookupResult{
            img_view.view,
            img_view.layout,
            img_view.format
        };

    read_only.scene_timestamp = scene_timestamp;

    // use prerender cmd as we can't copy an image or use pipeline barriers in a render pass
    VKContext *context = reinterpret_cast<VKContext *>(state.context);
    vk::CommandBuffer cmd_buffer = context->prerender_cmd;

    delta_row_samples *= state.res_multiplier;
    delta_col_samples *= state.res_multiplier;

    read_only.depth_view.transition_to_discard(cmd_buffer, vkutil::ImageLayout::TransferDst, vkutil::ds_subresource_range);

    cached_info.texture.transition_to(cmd_buffer, vkutil::ImageLayout::TransferSrc, vkutil::ds_subresource_range);
    vk::ImageSubresourceLayers layers = vkutil::color_subresource_layer;
    layers.aspectMask = vk::ImageAspectFlagBits::eDepth | vk::ImageAspectFlagBits::eStencil;
    vk::ImageCopy image_copy{
        .srcSubresource = layers,
        .srcOffset = { static_cast<int>(delta_col_samples), static_cast<int>(delta_row_samples), 0 },
        .dstSubresource = layers,
        .dstOffset = { 0, 0, 0 },
        .extent = { std::min(width, cached_info.texture.width - delta_col_samples), std::min(height, cached_info.texture.height - delta_row_samples), 1U }
    };
    cmd_buffer.copyImage(cached_info.texture.image, vk::ImageLayout::eTransferSrcOptimal, read_only.depth_view.image, vk::ImageLayout::eTransferDstOptimal, image_copy);

    // transition back
    cached_info.texture.transition_to(cmd_buffer, vkutil::ImageLayout::DepthStencilReadOnly, vkutil::ds_subresource_range);
    read_only.depth_view.transition_to(cmd_buffer, vkutil::ImageLayout::SampledImage, vkutil::ds_subresource_range);

    return TextureLookupResult{
        img_view.view,
        img_view.layout,
        img_view.format
    };
}

static Framebuffer empty_framebuffer{};
Framebuffer &VKSurfaceCache::retrieve_framebuffer_handle(MemState &mem, SceGxmColorSurface *color, SceGxmDepthStencilSurface *depth_stencil,
    vk::RenderPass standard_render_pass, vk::RenderPass interlock_render_pass, vk::ImageView &color_view, vk::ImageView &ds_view) {
    if (!target) {
        LOG_ERROR("Unable to retrieve framebuffer with no active render target!");
        return empty_framebuffer;
    }

    if (!color && !depth_stencil)
        LOG_ERROR_ONCE("Depth stencil and color surface are both null!");

    // might get modified by retrieve_color_surface_for_framebuffer
    state.pipeline_cache.can_use_deferred_compilation = true;

    // First retrieve separately the color surface and ds surface
    SurfaceRetrieveResult color_result;
    SurfaceRetrieveResult ds_result;

    if (color) {
        color_result = retrieve_color_surface_for_framebuffer(mem, color);
    } else {
        color_result.view = target->color.view;
        color_result.base_image = &target->color;
    }

    if (depth_stencil) {
        ds_result = retrieve_depth_stencil_for_framebuffer(depth_stencil, target->width, target->height);
    } else {
        ds_result.view = target->depthstencil.view;
        ds_result.base_image = &target->depthstencil;
    }

    color_view = color_result.view;
    ds_view = ds_result.view;

    std::pair<vk::ImageView, vk::ImageView> key = { color_view, ds_view };
    auto it = framebuffer_array.find(key);

    if (it != framebuffer_array.end()) {
        // we already created a framebuffer for this pair
        return it->second;
    }

    // make the framebuffer as big as possible
    const uint32_t framebuffer_width = std::min(color_result.base_image->width, ds_result.base_image->width);
    const uint32_t framebuffer_height = std::min(color_result.base_image->height, ds_result.base_image->height);

    vk::FramebufferCreateInfo fb_info{
        .renderPass = standard_render_pass,
        .width = framebuffer_width,
        .height = framebuffer_height,
        .layers = 1
    };
    vk::ImageView attachments[] = { color_result.view, ds_result.view };
    fb_info.setAttachments(attachments);
    vk::Framebuffer fb_standard = state.device.createFramebuffer(fb_info);

    vk::Framebuffer fb_interlock = nullptr;
    if (state.features.support_shader_interlock) {
        // we also need to create the framebuffer for shader interlock
        fb_info.renderPass = interlock_render_pass;
        fb_info.pAttachments = &attachments[1];
        fb_info.attachmentCount = 1;
        fb_interlock = state.device.createFramebuffer(fb_info);
    }

    return (framebuffer_array[key] = { fb_standard, fb_interlock, color_result.base_image });
}

bool VKSurfaceCache::check_for_surface(MemState &mem, Address source_address, CallbackRequestFunction &callback, Address target_address) {
    if (!state.features.enable_memory_mapping || state.disable_surface_sync)
        return false;

    if (vector_utils::find_index(cpu_surfaces_changed, source_address) != -1) {
        // there is a transfer operation pending on this surface, just add the callback after and we are done
        state.request_queue.push(CallbackRequest{ new CallbackRequestFunction(std::move(callback)) });

        if (target_address)
            cpu_surfaces_changed.push_back(target_address);
        return true;
    }

    // for now, only look if the address matches exactly a color surface
    auto it = color_address_lookup.find(source_address);
    if (it == color_address_lookup.end())
        return false;

    auto &surface = *it->second;
    VKContext &context = *static_cast<VKContext *>(state.context);
    // if the frame is already rendered skip
    // Note: that's not the best behavior but it should be fine
    // also it prevents invalidated surfaces from causing issues
    if (surface.last_frame_rendered + MAX_FRAMES_RENDERING <= context.frame_timestamp)
        return false;

    // we found something
    if (!*surface.need_surface_sync) {
        // first send the command to sync the surface with the GPU
        *surface.need_surface_sync = true;

        // we shouldn't have a command buffer being used, but just in case
        vk::CommandBuffer prev_cmd = context.render_cmd;

        // for the time being, just create a temp command buffer / fence
        // That's not the best approach but I guess it works
        vk::CommandBuffer surface_cmd = nullptr;
        vk::Fence fence = state.device.createFence({});
        ColorSurfaceCacheInfo *returned_info = nullptr;
        {
            std::lock_guard<std::mutex> lock(state.multithread_pool_mutex);
            surface_cmd = vkutil::create_single_time_command(state.device, state.multithread_command_pool);

            context.render_cmd = surface_cmd;
            last_written_surface = &surface;
            returned_info = perform_surface_sync();
            context.render_cmd = prev_cmd;

            surface_cmd.end();
        }
        // submit this command
        vk::SubmitInfo submit_info{};
        submit_info.setCommandBuffers(surface_cmd);
        state.general_queue.submit(submit_info, fence);

        // now we need to wait for the fence, then destroy it along with the command buffer
        // to prevent memory leaks
        CallbackRequestFunction vk_callback = [&state = this->state, fence, surface_cmd]() {
            auto result = state.device.waitForFences(fence, vk::True, std::numeric_limits<uint64_t>::max());
            if (result != vk::Result::eSuccess)
                LOG_ERROR("Could not wait for fences.");

            // destroy the objects
            state.device.destroyFence(fence);

            std::lock_guard<std::mutex> lock(state.multithread_pool_mutex);
            state.device.freeCommandBuffers(state.multithread_command_pool, surface_cmd);
        };
        state.request_queue.push(CallbackRequest{ new CallbackRequestFunction(std::move(vk_callback)) });

        if (returned_info)
            state.request_queue.push(PostSurfaceSyncRequest{ returned_info });
    }

    // now push the callback
    state.request_queue.push(CallbackRequest{ new CallbackRequestFunction(std::move(callback)) });

    if (target_address)
        cpu_surfaces_changed.push_back(target_address);

    return true;
}

ColorSurfaceCacheInfo *VKSurfaceCache::perform_surface_sync() {
    // surface sync is supported only if memory mapping is enabled
    if (!state.features.enable_memory_mapping)
        return nullptr;

    if (last_written_surface == nullptr || !*last_written_surface->need_surface_sync)
        return nullptr;

    VKContext *context = reinterpret_cast<VKContext *>(state.context);
    vk::CommandBuffer cmd_buffer = context->render_cmd;

    vk::Image image_to_copy = last_written_surface->texture.image;
    vk::ImageLayout image_layout = vk::ImageLayout::eGeneral;

    // this works for surface swizzles
    bool is_swizzle_identity = last_written_surface->swizzle.r == vk::ComponentSwizzle::eR;
    if (!is_swizzle_identity && !format_support_swizzle(last_written_surface->format)) {
        LOG_WARN_ONCE("Surface sync with swizzle not support on {}", vk::to_string(last_written_surface->texture.format));

        is_swizzle_identity = true;
    }

    if (state.res_multiplier != 1.0f) {
        // scale back the image using a blit command first

        if (!last_written_surface->blit_image)
            last_written_surface->blit_image = std::make_unique<vkutil::Image>();

        vkutil::Image &blit_image = *last_written_surface->blit_image;

        if (!blit_image.image) {
            blit_image.format = last_written_surface->texture.format;
            blit_image.width = last_written_surface->original_width;
            blit_image.height = last_written_surface->original_height;

            blit_image.init_image(vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst);
            blit_image.transition_to(cmd_buffer, vkutil::ImageLayout::TransferDst);
        } else {
            blit_image.transition_to_discard(cmd_buffer, vkutil::ImageLayout::TransferDst);
        }

        vk::ImageBlit blit{
            .srcSubresource = vkutil::color_subresource_layer,
            .srcOffsets = std::array<vk::Offset3D, 2>{ vk::Offset3D{ 0, 0, 0 }, vk::Offset3D{ last_written_surface->width, last_written_surface->height, 1 } },
            .dstSubresource = vkutil::color_subresource_layer,
            .dstOffsets = std::array<vk::Offset3D, 2>{ vk::Offset3D{ 0, 0, 0 }, vk::Offset3D{ last_written_surface->original_width, last_written_surface->original_height, 1 } },
        };
        // Apply nearest filter for the time being, linear might be better if we have no data in the texture tho
        cmd_buffer.blitImage(image_to_copy, image_layout, blit_image.image, vk::ImageLayout::eTransferDstOptimal, blit, vk::Filter::eNearest);

        blit_image.transition_to(cmd_buffer, vkutil::ImageLayout::TransferSrc);
        image_to_copy = blit_image.image;
        image_layout = vk::ImageLayout::eTransferSrcOptimal;
    }

    vk::Buffer buffer;
    uint32_t offset;
    if (format_need_additional_memory(last_written_surface->format)) {
        if (!last_written_surface->copy_buffer)
            last_written_surface->copy_buffer = std::make_unique<vkutil::Buffer>();

        vkutil::Buffer &copy_buffer = *last_written_surface->copy_buffer;

        if (!copy_buffer.buffer) {
            copy_buffer.size = last_written_surface->stride_bytes * last_written_surface->original_height;
            copy_buffer.init_buffer(vk::BufferUsageFlagBits::eTransferDst, vkutil::vma_mapped_alloc);
        }

        buffer = copy_buffer.buffer;
        offset = 0;

        last_written_surface->need_buffer_sync = false;
        last_written_surface->need_post_surface_sync = true;
    } else {
        last_written_surface->need_buffer_sync = true;
        last_written_surface->need_post_surface_sync = !is_swizzle_identity;
        std::tie(buffer, offset) = state.get_matching_mapping(last_written_surface->data);
    }
    const uint32_t pixel_stride = (last_written_surface->stride_bytes * 8) / gxm::bits_per_pixel(last_written_surface->format);
    vk::BufferImageCopy copy{
        .bufferOffset = offset,
        .bufferRowLength = pixel_stride,
        .bufferImageHeight = last_written_surface->original_height,
        .imageSubresource = vkutil::color_subresource_layer,
        .imageOffset = { 0, 0, 0 },
        .imageExtent = { last_written_surface->original_width, last_written_surface->original_height, 1 }
    };
    cmd_buffer.copyImageToBuffer(image_to_copy, image_layout, buffer, copy);

    ColorSurfaceCacheInfo *return_value = last_written_surface;
    last_written_surface = nullptr;

    return return_value;
}

template <typename T>
static void swizzle_text_T_2(T *pixels, uint32_t nb_pixel) {
    for (uint32_t i = 0; i < nb_pixel; i++) {
        std::swap(pixels[2 * i], pixels[2 * i + 1]);
    }
}

template <typename T, size_t type>
static void swizzle_text_T_4(T *pixels, uint32_t nb_pixel) {
    for (uint32_t i = 0; i < nb_pixel; i++) {
        if constexpr (type == 0) {
            // BGRA
            std::swap(pixels[4 * i], pixels[4 * i + 2]);
        } else if constexpr (type == 1) {
            // ABGR
            std::swap(pixels[4 * i], pixels[4 * i + 3]);
            std::swap(pixels[4 * i + 1], pixels[4 * i + 2]);
        } else {
            // ARGB
            T copy[] = { pixels[4 * i],
                pixels[4 * i + 1],
                pixels[4 * i + 2],
                pixels[4 * i + 3] };
            pixels[4 * i] = copy[3];
            pixels[4 * i + 1] = copy[0];
            pixels[4 * i + 2] = copy[1];
            pixels[4 * i + 3] = copy[2];
        }
    }
}

template <typename T>
static void swizzle_text_T(T *pixels, uint32_t nb_pixel, ColorSurfaceCacheInfo *surface) {
    // there can only be 2 or 4 component textures here
    if (vk::componentCount(surface->texture.format) == 2) {
        swizzle_text_T_2<T>(pixels, nb_pixel);
    } else {
        // find the swizzle
        // swizzles are inversed
        switch (surface->swizzle.r) {
        case vk::ComponentSwizzle::eB:
            // BGRA
            swizzle_text_T_4<T, 0>(pixels, nb_pixel);
            break;
        case vk::ComponentSwizzle::eA:
            // ABGR
            swizzle_text_T_4<T, 1>(pixels, nb_pixel);
            break;
        case vk::ComponentSwizzle::eG:
            // ARGB
            swizzle_text_T_4<T, 2>(pixels, nb_pixel);
            break;
        }
    }
}

void VKSurfaceCache::perform_post_surface_sync(const MemState &mem, ColorSurfaceCacheInfo *surface) {
    if (surface == nullptr)
        return;

    const uint32_t pixel_stride = (surface->stride_bytes * 8) / gxm::bits_per_pixel(surface->format);
    const uint32_t nb_pixels = pixel_stride * surface->original_height;
    uint8_t *pixels = surface->data.cast<uint8_t>().get(mem);

    if (format_need_additional_memory(surface->format)) {
        // special case, use a custom function
        const bool is_swizzle_identity = surface->swizzle.r == vk::ComponentSwizzle::eR;
        if (!surface->sws_context) {
            const AVPixelFormat dst_fmt = is_swizzle_identity ? AV_PIX_FMT_RGB24 : AV_PIX_FMT_BGR24;
            surface->sws_context = sws_getContext(surface->original_width, surface->original_height, AV_PIX_FMT_RGB0, surface->original_width, surface->original_height, dst_fmt, 0, nullptr, nullptr, nullptr);
            assert(surface->sws_context != NULL);
        }

        int src_stride = pixel_stride * 4;
        int dst_stride = pixel_stride * 3;
        sws_scale(surface->sws_context, reinterpret_cast<const uint8_t *const *>(&surface->copy_buffer->mapped_data), &src_stride, 0, surface->original_height, &pixels, &dst_stride);
        return;
    }

    switch (vk::componentBits(surface->texture.format, 0)) {
    case 8:
        swizzle_text_T<uint8_t>(pixels, nb_pixels, surface);
        break;
    case 16:
        swizzle_text_T<uint16_t>(reinterpret_cast<uint16_t *>(pixels), nb_pixels, surface);
        break;
    case 32:
        swizzle_text_T<uint32_t>(reinterpret_cast<uint32_t *>(pixels), nb_pixels, surface);
        break;
    }
}

void VKSurfaceCache::destroy_associated_framebuffers(const VKRenderTarget *render_target) {
    destroy_framebuffers(render_target->color.view);
    destroy_framebuffers(render_target->depthstencil.view);
}

vk::ImageView VKSurfaceCache::sourcing_color_surface_for_presentation(Ptr<const void> address, uint32_t pitch, Viewport &viewport) {
    // get closest surface with an address below address
    auto ite = color_address_lookup.upper_bound(address.address());
    if (ite == color_address_lookup.begin()) {
        return nullptr;
    }
    --ite;

    ColorSurfaceCacheInfo &info = *ite->second;
    if (info.data.address() + info.total_bytes <= address.address())
        // they do not overlap
        return nullptr;

    if (info.stride_bytes == pitch * 4) {
        // In assumption the format is RGBA8
        const size_t data_delta = address.address() - ite->first;
        uint32_t limited_height = viewport.height;
        if ((data_delta % (pitch * 4)) == 0) {
            uint32_t start_sourced_line = static_cast<uint32_t>((data_delta / (pitch * 4)) * state.res_multiplier);
            if ((start_sourced_line + viewport.height) > info.height) {
                // Sometimes the surface is just missing a little bit of lines
                if (start_sourced_line < info.height) {
                    // Just limit the height and display it
                    limited_height = info.height - start_sourced_line;
                } else {
                    LOG_ERROR("Trying to present non-existent segment in cached color surface!");
                    return nullptr;
                }
            }

            // Compute position in texture
            viewport.offset_x = 0;
            viewport.offset_y = start_sourced_line;
            viewport.width = std::min(viewport.width, static_cast<uint32_t>(info.width));
            viewport.height = limited_height;
            viewport.texture_width = info.width;
            viewport.texture_height = info.height;

            if (info.swizzle == vkutil::rgba_mapping && info.texture.format == vk::Format::eR8G8B8A8Unorm)
                return info.texture.view;

            if (!info.alternate_view) {
                // create a view with the right swizzle and without gamma correction
                vk::ImageViewCreateInfo view_info{
                    .image = info.texture.image,
                    .viewType = vk::ImageViewType::e2D,
                    .format = vk::Format::eR8G8B8A8Unorm,
                    .components = vkutil::color_to_texture_swizzle(info.swizzle, vkutil::rgba_mapping),
                    .subresourceRange = vkutil::color_subresource_range
                };
                info.alternate_view = state.device.createImageView(view_info);
            }

            return info.alternate_view;
        }
    }

    return nullptr;
}

std::vector<uint32_t> VKSurfaceCache::dump_frame(Ptr<const void> address, uint32_t width, uint32_t height, uint32_t pitch) {
    // get closest surface with an address below address
    auto ite = color_address_lookup.upper_bound(address.address());
    if (ite == color_address_lookup.begin()) {
        return {};
    }
    --ite;

    const ColorSurfaceCacheInfo &info = *ite->second;

    const uint32_t data_delta = address.address() - ite->first;
    const uint32_t pitch_byte = pitch * 4;
    if (info.stride_bytes != pitch_byte || data_delta % pitch_byte != 0)
        return {};

    const uint32_t line_delta = static_cast<uint32_t>((data_delta / pitch_byte) * state.res_multiplier);
    if (line_delta >= info.height)
        return {};

    const uint32_t real_height = std::min(height, info.height - line_delta);

    std::vector<uint32_t> frame(width * height, 0);

    // we need a temporary buffer and command buffer for this
    // this is a raii buffer, it will be destroyed at the end of this function
    vkutil::Buffer temp_buff(width * height * 4);
    temp_buff.init_buffer(vk::BufferUsageFlagBits::eTransferDst, vkutil::vma_mapped_alloc);
    vk::CommandBuffer cmd_buffer = vkutil::create_single_time_command(state.device, state.general_command_pool);

    // layout is general, we can directly copy from it
    vk::BufferImageCopy image_copy{
        .bufferOffset = 0,
        .bufferRowLength = width,
        .bufferImageHeight = height,
        .imageSubresource = vkutil::color_subresource_layer,
        .imageOffset = { 0, static_cast<int>(line_delta), 0 },
        .imageExtent = { width, real_height, 1 }
    };
    cmd_buffer.copyImageToBuffer(info.texture.image, vk::ImageLayout::eGeneral, temp_buff.buffer, image_copy);

    // this will cause a waitIdle, not an issue
    vkutil::end_single_time_command(state.device, state.general_queue, state.general_command_pool, cmd_buffer);

    memcpy(frame.data(), temp_buff.mapped_data, frame.size() * 4);

    return frame;
}

} // namespace renderer::vulkan
