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

#include <renderer/vulkan/surface_cache.h>

#include <gxm/functions.h>
#include <renderer/vulkan/gxm_to_vulkan.h>
#include <renderer/vulkan/state.h>
#include <renderer/vulkan/types.h>
#include <vkutil/vkutil.h>

#include <vulkan/vulkan_format_traits.hpp>

#include <util/align.h>
#include <util/keywords.h>
#include <util/log.h>

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

static constexpr std::uint64_t CASTED_UNUSED_TEXTURE_PURGE_SECS = 40;

ColorSurfaceCacheInfo::~ColorSurfaceCacheInfo() {
    sws_freeContext(sws_context);
}

void VKSurfaceCache::destroy_framebuffers(vk::ImageView view) {
    vkutil::DestroyQueue &destroy_queue = reinterpret_cast<VKContext *>(state.context)->frame().destroy_queue;
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
    VKContext *context = reinterpret_cast<VKContext *>(state.context);
    vkutil::DestroyQueue &destroy_queue = context->frame().destroy_queue;

    // don't forget to destroy in the right order
    for (auto &casted : info.casted_textures) {
        destroy_queue.add_buffer(casted.transition_buffer);
        destroy_queue.add_image(casted.texture);
    }

    if (info.sampled_image) {
        // make sure it is not destroyed twice (if set info.sampled_image.image is the same as info.texture.image)
        info.sampled_image->image = nullptr;
        destroy_queue.add_image(*info.sampled_image);
    }
    destroy_framebuffers(info.texture.view);
    destroy_queue.add_image(info.texture);
}

void VKSurfaceCache::destroy_surface(DepthStencilSurfaceCacheInfo &info) {
    VKContext *context = reinterpret_cast<VKContext *>(state.context);
    vkutil::DestroyQueue &destroy_queue = context->frame().destroy_queue;

    for (auto &read_only : info.read_surfaces)
        destroy_queue.add_image(read_only.depth_view);

    destroy_framebuffers(info.texture.view);
    destroy_queue.add_image(info.texture);
}

VKSurfaceCache::VKSurfaceCache(VKState &state)
    : state(state) {
    for (int i = 0; i < MAX_CACHE_SIZE_PER_CONTAINER; i++) {
        depth_stencil_textures[i].surface.depth_data = -1;
        depth_stencil_textures[i].surface.stencil_data = -1;
        depth_stencil_textures[i].flags = SurfaceCacheInfo::FLAG_FREE;
    }
}

vkutil::Image *VKSurfaceCache::retrieve_color_surface_texture_handle(MemState &mem, uint16_t width, uint16_t height, const uint16_t pixel_stride,
    const SceGxmColorBaseFormat base_format, const SceGxmColorSurfaceType surface_type, const bool is_srgb, Ptr<void> address, SurfaceTextureRetrievePurpose purpose, vk::ComponentMapping &swizzle,
    uint16_t *stored_height, uint16_t *stored_width, TextureViewport *texture_viewport) {
    // Create the key to access the cache struct
    const std::uint64_t key = address.address();

    const uint32_t original_width = width;
    const uint32_t original_height = height;

    width *= state.res_multiplier;
    height *= state.res_multiplier;

    bool overlap = true;

    // Of course, this works under the assumption that range must be unique :D
    auto ite = color_surface_textures.upper_bound(key);
    if (ite == color_surface_textures.begin())
        // no match
        overlap = false;
    else
        ite--;
    // ite is now the first item with an adress lower or equal to key
    bool invalidated = false;

    overlap = (overlap && (ite->first + ite->second.total_bytes) > key);

    if (!overlap && purpose != SurfaceTextureRetrievePurpose::WRITING) {
        // not part of a surface, let the texture cache handle it
        return nullptr;
    }
    vk::Format vk_format = color::translate_format(base_format);
    if (is_srgb) {
        if (vk_format == vk::Format::eR8G8B8A8Unorm) {
            vk_format = vk::Format::eR8G8B8A8Srgb;
        } else {
            LOG_WARN_ONCE("Trying to use gamma correction with non-compatible format {}", vk::to_string(vk_format));
        }
    }

    uint32_t bytes_per_stride = pixel_stride * gxm::bits_per_pixel(base_format) / 8;
    uint32_t total_surface_size = bytes_per_stride * original_height;

    if (overlap) {
        ColorSurfaceCacheInfo &info = ite->second;

        if (purpose == SurfaceTextureRetrievePurpose::READING
            && (base_format == SCE_GXM_COLOR_BASE_FORMAT_U8U8U8 || info.format == SCE_GXM_COLOR_BASE_FORMAT_U8U8U8)
            && base_format != info.format)
            // don't even try to match u8u8u8 with something else
            return nullptr;

        auto used_iterator = std::find(last_use_color_surface_index.begin(), last_use_color_surface_index.end(), ite->first);

        if (stored_width) {
            *stored_width = info.original_width;
        }

        if (stored_height) {
            *stored_height = info.original_height;
        }

        // There are four situations I think of:
        // 1. Different base address, lookup for write, in this case, if the cached surface range contains the given address, then
        // probably this cached surface has already been freed GPU-wise. So erase.
        // 2. Same base address, but width and height change to be larger, or format change if write. Remake a new one for both read and write sitatation.
        // 3. Out of cache range. In write case, create a new one, in read case, lul
        // 4. Read situation with smaller width and height, probably need to extract the needed region out.
        // 5. the surface is a gbuffer and we are currently trying to read the 2nd component, in this case key == ite->first + 4
        const bool addr_in_range_of_cache = ((key + total_surface_size) <= (ite->first + info.total_bytes + 4));
        const bool cache_probably_freed = ((ite->first != key) && addr_in_range_of_cache && (purpose == SurfaceTextureRetrievePurpose::WRITING));
        const bool surface_extent_changed = (info.height < height);
        bool surface_stat_changed = false;

        if (ite->first == key) {
            if (purpose == SurfaceTextureRetrievePurpose::WRITING) {
                surface_stat_changed = surface_extent_changed || info.width < width || base_format != info.format;
            } else {
                // If the extent changed but format is not the same, then the probability of it being a cast is high
                surface_stat_changed = (surface_extent_changed || info.pixel_stride < pixel_stride) && base_format == info.format;
            }
        }

        if (cache_probably_freed || surface_stat_changed) {
            // Clear out. We will recreate later

            destroy_surface(ite->second);
            color_surface_textures.erase(ite);
            invalidated = true;
        } else if (!addr_in_range_of_cache) {
            if (purpose == SurfaceTextureRetrievePurpose::WRITING) {
                destroy_surface(ite->second);
                color_surface_textures.erase(ite);
                invalidated = true;
            }
        } else if (purpose == SurfaceTextureRetrievePurpose::READING) {
            // If we read and it's still in range
            if (used_iterator != last_use_color_surface_index.end()) {
                last_use_color_surface_index.erase(used_iterator);
            }

            last_use_color_surface_index.push_back(ite->first);

            if (info.flags & SurfaceCacheInfo::FLAG_DIRTY) {
                // We can't use this texture sadly :( If it uses for writing of course it will be gud gud
                return nullptr;
            }

            bool castable = (info.pixel_stride == pixel_stride);

            uint32_t bytes_per_pixel_requested = gxm::bits_per_pixel(base_format) / 8;
            uint32_t bytes_per_pixel_in_store = gxm::bits_per_pixel(info.format) / 8;

            // Check if castable. Technically the income format should be texture format, but this is for easier logic.
            // When it's required. I may change :p
            if (base_format != info.format) {
                if (bytes_per_pixel_requested > bytes_per_pixel_in_store) {
                    castable = (((bytes_per_pixel_requested % bytes_per_pixel_in_store) == 0) && (info.pixel_stride % pixel_stride == 0) && ((info.pixel_stride / pixel_stride) == (bytes_per_pixel_requested / bytes_per_pixel_in_store)));
                } else {
                    castable = (((bytes_per_pixel_in_store % bytes_per_pixel_requested) == 0) && (pixel_stride % info.pixel_stride == 0) && ((pixel_stride / info.pixel_stride) == (bytes_per_pixel_in_store / bytes_per_pixel_requested)));
                }

                if (!castable)
                    return nullptr;
            }
            if (castable) {
                // TODO: this is true only for linear textures (and also kind of for tiled textures) (and in this case start_x = 0),
                // for swizzled textures this is different
                const uint32_t data_delta = address.address() - ite->first;
                uint32_t start_sourced_line = (data_delta / bytes_per_stride) * state.res_multiplier;
                uint32_t start_x = (data_delta % bytes_per_stride) / bytes_per_pixel_requested * state.res_multiplier;

                if (static_cast<uint16_t>(start_sourced_line + height) > info.height) {
                    LOG_ERROR("Trying to present non-existen segment in cached color surface!");
                    return 0;
                }

                const vk::ImageView color_handle_view = reinterpret_cast<VKContext *>(state.context)->current_color_attachment->view;
                const bool is_same_image = (color_handle_view == info.texture.view) || (info.sampled_image && color_handle_view == info.sampled_image->view);

                if (state.features.use_texture_viewport && vk_format == info.texture.format) {
                    // use a texture viewport
                    *texture_viewport = {
                        .ratio = {
                            original_width / static_cast<float>(info.original_width),
                            original_height / static_cast<float>(info.original_height) },
                        .offset = { start_x / static_cast<float>(info.width), start_sourced_line / static_cast<float>(info.height) }
                    };
                    return &info.texture;
                }

                if (is_same_image || (start_sourced_line != 0) || (start_x != 0) || (info.width != width) || (info.height != height) || (info.format != base_format)) {
                    uint64_t current_time = std::chrono::duration_cast<std::chrono::seconds>(
                        std::chrono::steady_clock::now().time_since_epoch())
                                                .count();

                    const uint64_t scene_timestamp = reinterpret_cast<VKContext *>(state.context)->scene_timestamp;

                    std::vector<CastedTexture> &casted_vec = info.casted_textures;

                    CastedTexture *casted = nullptr;

                    // Look in cast cache and grab one. The cache really does not store immediate grab on now, but rather to reduce the synchronization in the pipeline (use different texture)
                    for (size_t i = 0; i < casted_vec.size();) {
                        if ((casted_vec[i].cropped_height == height) && (casted_vec[i].cropped_width == width) && (casted_vec[i].cropped_y == start_sourced_line) && (casted_vec[i].cropped_x == start_x) && (casted_vec[i].format == base_format)) {
                            casted = &casted_vec[i];

                            if (casted->scene_timestamp == scene_timestamp) {
                                // already copied for this scene, don't do it again
                                return &casted->texture;
                            }

                            break;
                        } else if (current_time - info.casted_textures[i].last_used_time >= CASTED_UNUSED_TEXTURE_PURGE_SECS) {
                            casted_vec.erase(casted_vec.begin() + i);
                            continue;
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
                            .last_used_time = current_time,
                            .cropped_x = start_x,
                            .cropped_y = start_sourced_line,
                            .cropped_width = width,
                            .cropped_height = height,
                            .format = base_format
                        };
                        casted->texture.allocator = state.allocator;
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

                    casted->last_used_time = current_time;
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
                                std::min(width, info.width),
                                height,
                                1 }
                        };
                        cmd_buffer.copyImage(info.texture.image, vk::ImageLayout::eGeneral, casted->texture.image, vk::ImageLayout::eTransferDstOptimal, image_copy);
                    } else {
                        LOG_INFO_ONCE("Game is doing typeless copies");
                        // We must use a transition buffer
                        vk::DeviceSize buffer_size = bytes_per_stride * state.res_multiplier * height + start_x * bytes_per_pixel_requested;
                        if (!casted->transition_buffer.buffer || casted->transition_buffer.size < buffer_size) {
                            // create or re-create the buffer
                            context->frame().destroy_queue.add_buffer(casted->transition_buffer);
                            casted->transition_buffer = vkutil::Buffer(state.allocator, buffer_size);
                            casted->transition_buffer.init_buffer(vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eTransferSrc);
                        }

                        // copy the image to the buffer
                        vk::BufferImageCopy copy_image_buffer{
                            .bufferOffset = 0,
                            .bufferRowLength = static_cast<uint32_t>(info.pixel_stride * state.res_multiplier),
                            .bufferImageHeight = height,
                            .imageSubresource = vkutil::color_subresource_layer,
                            .imageOffset = { 0,
                                static_cast<int32_t>(start_sourced_line),
                                0 },
                            .imageExtent = { info.width, height, 1 }
                        };
                        cmd_buffer.copyImageToBuffer(info.texture.image, vk::ImageLayout::eGeneral, casted->transition_buffer.buffer, copy_image_buffer);

                        // then the buffer to the image
                        copy_image_buffer
                            .setBufferOffset(start_x * bytes_per_pixel_requested)
                            .setBufferRowLength(pixel_stride * state.res_multiplier)
                            .setImageOffset({ 0, 0, 0 })
                            .setImageExtent({ width, height, 1 });
                        cmd_buffer.copyBufferToImage(casted->transition_buffer.buffer, casted->texture.image, vk::ImageLayout::eTransferDstOptimal, copy_image_buffer);
                    }
                    casted->texture.transition_to(cmd_buffer, vkutil::ImageLayout::ColorAttachmentReadWrite);

                    return &casted->texture;
                } else {
                    // we must insert a barrier before reading from the texture
                    VKContext *context = reinterpret_cast<VKContext *>(state.context);
                    vk::CommandBuffer cmd_buffer = context->prerender_cmd;

                    if (swizzle == info.swizzle && vk_format == info.texture.format)
                        // we can use the same texture view
                        return &info.texture;

                    if (!info.sampled_image)
                        info.sampled_image = std::make_unique<vkutil::Image>();

                    if (!info.sampled_image->view) {
                        vk::ComponentMapping resulting_mapping = vkutil::color_to_texture_swizzle(info.swizzle, swizzle);

                        vk::ImageViewCreateInfo view_info{
                            .image = info.texture.image,
                            .viewType = vk::ImageViewType::e2D,
                            .format = vk_format,
                            .components = resulting_mapping,
                            .subresourceRange = vkutil::color_subresource_range
                        };
                        info.sampled_image->view = state.device.createImageView(view_info);
                        info.sampled_image->layout = vkutil::ImageLayout::ColorAttachmentReadWrite;
                        info.sampled_image->format = vk_format;
                    }

                    return info.sampled_image.get();
                }
            }
        }

        if (!invalidated) {
            if (purpose == SurfaceTextureRetrievePurpose::WRITING) {
                if (used_iterator != last_use_color_surface_index.end()) {
                    last_use_color_surface_index.erase(used_iterator);
                }

                last_use_color_surface_index.push_back(ite->first);
                last_written_surface = &info;

                if (vk_format == info.texture.format) {
                    return &info.texture;
                } else {
                    // using both srgb/linear
                    if (!info.sampled_image)
                        info.sampled_image = std::make_unique<vkutil::Image>();

                    if (!info.sampled_image->view) {
                        vk::ImageViewCreateInfo view_info{
                            .image = info.texture.image,
                            .viewType = vk::ImageViewType::e2D,
                            .format = vk_format,
                            .components = vkutil::default_comp_mapping,
                            .subresourceRange = vkutil::color_subresource_range
                        };
                        info.sampled_image->view = state.device.createImageView(view_info);
                        info.sampled_image->layout = vkutil::ImageLayout::ColorAttachmentReadWrite;
                        info.sampled_image->format = vk_format;
                    }

                    return info.sampled_image.get();
                }
            } else {
                return nullptr;
            }
        } else if (used_iterator != last_use_color_surface_index.end()) {
            last_use_color_surface_index.erase(used_iterator);
        }
    }

    VKContext *context = reinterpret_cast<VKContext *>(state.context);
    ColorSurfaceCacheInfo &info_added = color_surface_textures[key];

    if (info_added.texture.image) {
        // deferred destruction of the existing surface
        destroy_surface(info_added);
    }

    info_added.width = width;
    info_added.height = height;
    info_added.original_width = original_width;
    info_added.original_height = original_height;
    info_added.pixel_stride = pixel_stride;
    info_added.data = address;
    info_added.total_bytes = bytes_per_stride * original_height;
    info_added.format = base_format;
    // only remember the swizzle here, it will be useful if we get to present or sample from this image with a different swizzle
    info_added.swizzle = swizzle;
    info_added.flags = 0;

    vkutil::Image &image = info_added.texture;
    image.allocator = state.allocator;
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

    // Now that everything goes well, we can start rearranging
    if (last_use_color_surface_index.size() >= MAX_CACHE_SIZE_PER_CONTAINER) {
        // We have to purge a cache along with framebuffer
        // So choose the one that is last used
        const std::uint64_t first_key = last_use_color_surface_index.front();
        if (first_key != key) {
            destroy_surface(color_surface_textures[first_key]);
            color_surface_textures.erase(first_key);
        }

        last_use_color_surface_index.erase(last_use_color_surface_index.begin());
    }

    last_use_color_surface_index.push_back(key);

    if (stored_height) {
        *stored_height = height;
    }

    if (stored_width) {
        *stored_width = width;
    }

    last_written_surface = &info_added;
    info_added.need_surface_sync.reset();
    info_added.need_surface_sync = std::make_shared<bool>();
    *info_added.need_surface_sync = false;

    // we only support surface sync of linear surfaces for now
    if (!can_mprotect_mapped_memory) {
        // peform surface sync on everything
        // it is slow but well... we can't mprotect the buffer
        *info_added.need_surface_sync = surface_type == SCE_GXM_COLOR_SURFACE_LINEAR;
    } else if (surface_type == SCE_GXM_COLOR_SURFACE_LINEAR && format_support_surface_sync(base_format)) {
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

    return &info_added.texture;
}

vkutil::Image *VKSurfaceCache::retrieve_depth_stencil_texture_handle(const MemState &mem, const SceGxmDepthStencilSurface &surface, int32_t width,
    int32_t height, const bool is_reading, TextureViewport *texture_viewport) {
    bool packed_ds = surface.get_format() == SCE_GXM_DEPTH_STENCIL_FORMAT_S8D24;

    int32_t memory_width = width;
    int32_t memory_height = height;

    if (!is_reading) {
        // when writing we use the render target size which is already upscaled
        memory_width /= state.res_multiplier;
        memory_height /= state.res_multiplier;

        // check if MSAA is used, the depth buffer is never downscaled
        if (target->multisample_mode != SCE_GXM_MULTISAMPLE_NONE)
            memory_width *= 2;
        if (target->multisample_mode == SCE_GXM_MULTISAMPLE_4X)
            memory_height *= 2;

    } else {
        // take upscaling into account
        width *= state.res_multiplier;
        height *= state.res_multiplier;
    }

    const bool is_stencil_only = surface.depth_data.address() == 0;
    size_t found_index = -1;

    // The whole depth stencil struct is reserved for future use
    for (size_t i = 0; i < depth_stencil_textures.size(); i++) {
        if ((!is_stencil_only && depth_stencil_textures[i].surface.depth_data == surface.depth_data)
            || (is_stencil_only && depth_stencil_textures[i].surface.stencil_data == surface.stencil_data)
            || (is_reading && !packed_ds && surface.depth_data == depth_stencil_textures[i].surface.stencil_data)) {
            found_index = i;
            break;
        }
    }

    if (found_index != static_cast<std::size_t>(-1)) {
        auto ite = std::find(last_use_depth_stencil_surface_index.begin(), last_use_depth_stencil_surface_index.end(), found_index);
        if (ite != last_use_depth_stencil_surface_index.end()) {
            last_use_depth_stencil_surface_index.erase(ite);
            last_use_depth_stencil_surface_index.push_back(found_index);
        }

        DepthStencilSurfaceCacheInfo &cached_info = depth_stencil_textures[found_index];
        bool need_remake = false;
        if (is_reading) {
            if (cached_info.memory_width < memory_width || cached_info.memory_height < memory_height)
                return nullptr;
        } else {
            need_remake = cached_info.texture.width < width || cached_info.texture.height < height;
        }

        if (!need_remake) {
            if (!is_reading)
                return &cached_info.texture;

            // take MSAA into account
            if (cached_info.multisample_mode != SCE_GXM_MULTISAMPLE_NONE)
                width /= 2;
            if (cached_info.multisample_mode == SCE_GXM_MULTISAMPLE_4X)
                height /= 2;

            const bool is_stencil = depth_stencil_textures[found_index].surface.depth_data != surface.depth_data;

            vk::ImageView ds_attachment = reinterpret_cast<VKContext *>(state.context)->current_ds_attachment->view;
            const bool reading_ds_attachment = cached_info.texture.view == ds_attachment;

            if (state.features.use_texture_viewport && !reading_ds_attachment) {
                // use a texture viewport

                // we must create a new read-only view if it is not already present
                // with a texture viewport, we will only use one read surface at most
                if (cached_info.read_surfaces.empty())
                    cached_info.read_surfaces.resize(1);
                vkutil::Image &img_view = is_stencil ? cached_info.read_surfaces[0].stencil_view : cached_info.read_surfaces[0].depth_view;
                if (!img_view.view) {
                    vk::ImageSubresourceRange range = vkutil::ds_subresource_range;
                    range.aspectMask = is_stencil ? vk::ImageAspectFlagBits::eStencil : vk::ImageAspectFlagBits::eDepth;
                    vk::ImageViewCreateInfo view_info{
                        .image = cached_info.texture.image,
                        .viewType = vk::ImageViewType::e2D,
                        .format = vk::Format::eD32SfloatS8Uint,
                        .components = {},
                        .subresourceRange = range
                    };
                    img_view.view = state.device.createImageView(view_info);
                    img_view.layout = vkutil::ImageLayout::DepthReadOnly;
                }

                texture_viewport->ratio = {
                    memory_width / static_cast<float>(cached_info.memory_width),
                    memory_height / static_cast<float>(cached_info.memory_height)
                };
                return &img_view;
            }

            const uint64_t scene_timestamp = reinterpret_cast<VKContext *>(state.context)->scene_timestamp;

            int read_surface_idx = -1;
            for (int i = 0; i < cached_info.read_surfaces.size(); i++) {
                if (cached_info.read_surfaces[i].depth_view.width == width
                    && cached_info.read_surfaces[i].depth_view.height == height) {
                    read_surface_idx = i;
                    break;
                }
            }

            if (read_surface_idx == -1) {
                // no compatible read surface found

                DepthSurfaceView read_only{
                    .depth_view = vkutil::Image(state.allocator, width, height, vk::Format::eD32SfloatS8Uint),
                    .scene_timestamp = 0
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
                return &img_view;

            read_only.scene_timestamp = scene_timestamp;

            // use prerender cmd as we can't copy an image or use pipeline barriers in a render pass
            VKContext *context = reinterpret_cast<VKContext *>(state.context);
            vk::CommandBuffer cmd_buffer = context->prerender_cmd;

            read_only.depth_view.transition_to_discard(cmd_buffer, vkutil::ImageLayout::TransferDst, vkutil::ds_subresource_range);

            cached_info.texture.transition_to(cmd_buffer, vkutil::ImageLayout::TransferSrc, vkutil::ds_subresource_range);
            vk::ImageSubresourceLayers layers = vkutil::color_subresource_layer;
            layers.aspectMask = vk::ImageAspectFlagBits::eDepth | vk::ImageAspectFlagBits::eStencil;
            vk::ImageCopy image_copy{
                .srcSubresource = layers,
                .srcOffset = { 0, 0, 0 },
                .dstSubresource = layers,
                .dstOffset = { 0, 0, 0 },
                .extent = { static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1U }
            };
            cmd_buffer.copyImage(cached_info.texture.image, vk::ImageLayout::eTransferSrcOptimal, read_only.depth_view.image, vk::ImageLayout::eTransferDstOptimal, image_copy);

            // transition back
            cached_info.texture.transition_to(cmd_buffer, vkutil::ImageLayout::DepthReadOnly, vkutil::ds_subresource_range);
            read_only.depth_view.transition_to(cmd_buffer, vkutil::ImageLayout::SampledImage, vkutil::ds_subresource_range);

            return &img_view;
        }
    }

    if (is_reading)
        return nullptr;

    // Now that everything goes well, we can start rearranging
    // Almost carbon copy but still too specific
    if (last_use_depth_stencil_surface_index.size() >= MAX_CACHE_SIZE_PER_CONTAINER) {
        // We have to purge a cache along with framebuffer
        // So choose the one that is last used
        const std::size_t index = last_use_depth_stencil_surface_index.front();

        last_use_depth_stencil_surface_index.erase(last_use_depth_stencil_surface_index.begin());
        depth_stencil_textures[index].flags = SurfaceCacheInfo::FLAG_FREE;

        found_index = index;
    }

    if (found_index == static_cast<std::size_t>(-1)) {
        // Still nowhere to found a free slot? We can search maybe
        for (std::size_t i = 0; i < depth_stencil_textures.size(); i++) {
            if (depth_stencil_textures[i].flags & SurfaceCacheInfo::FLAG_FREE) {
                found_index = i;
                break;
            }
        }
    }

    if (found_index == -1) {
        LOG_ERROR("No free depth stencil texture cache slot!");
        return nullptr;
    }

    if (depth_stencil_textures[found_index].texture.image) {
        // deferred deletion of the previous surface
        destroy_surface(depth_stencil_textures[found_index]);
    }

    last_use_depth_stencil_surface_index.push_back(found_index);
    DepthStencilSurfaceCacheInfo &cached_info = depth_stencil_textures[found_index];
    cached_info.flags = 0;
    cached_info.surface = surface;
    cached_info.memory_width = memory_width;
    cached_info.memory_height = memory_height;
    cached_info.multisample_mode = target->multisample_mode;

    vkutil::Image &image = cached_info.texture;

    // use prerender cmd in case we read from the depth buffer (although I really doubt this could happen)
    VKContext *context = reinterpret_cast<VKContext *>(state.context);
    vk::CommandBuffer cmd_buffer = context->prerender_cmd;

    image.allocator = state.allocator;
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
    image.transition_to(cmd_buffer, vkutil::ImageLayout::DepthReadOnly, vkutil::ds_subresource_range);

    return &image;
}

static Framebuffer empty_framebuffer{};
Framebuffer &VKSurfaceCache::retrieve_framebuffer_handle(MemState &mem, SceGxmColorSurface *color, SceGxmDepthStencilSurface *depth_stencil,
    vk::RenderPass standard_render_pass, vk::RenderPass interlock_render_pass, vkutil::Image **color_texture_handle, vkutil::Image **ds_texture_handle,
    uint16_t *stored_height, const uint32_t width, const uint32_t height) {
    if (!target) {
        LOG_ERROR("Unable to retrieve framebuffer with no active render target!");
        return empty_framebuffer;
    }

    if (!color && !depth_stencil) {
        LOG_ERROR_ONCE("Depth stencil and color surface are both null!");
        return empty_framebuffer;
    }

    // First retrieve separately the color surface and ds surface
    vkutil::Image *color_handle;
    vkutil::Image *ds_handle;

    if (!color_texture_handle) {
        // used when getting a shader interlock framebuffer
        color_handle = nullptr;
    } else if (color && color->data) {
        const SceGxmColorBaseFormat color_base_format = gxm::get_base_format(color->colorFormat);
        vk::ComponentMapping swizzle = color::translate_swizzle(color->colorFormat);
        color_handle = retrieve_color_surface_texture_handle(mem, color->width,
            color->height, color->strideInPixels, color_base_format, color->surfaceType, static_cast<bool>(color->gamma),
            color->data, renderer::SurfaceTextureRetrievePurpose::WRITING, swizzle, stored_height);
    } else {
        color_handle = &target->color;
    }

    if (depth_stencil && (depth_stencil->depth_data || depth_stencil->stencil_data)) {
        assert(target->width >= width && target->height >= height);
        ds_handle = retrieve_depth_stencil_texture_handle(mem, *depth_stencil, target->width, target->height);
    } else {
        ds_handle = &target->depthstencil;
    }

    if (color_texture_handle) {
        *color_texture_handle = color_handle;
    }

    if (ds_texture_handle) {
        *ds_texture_handle = ds_handle;
    }

    std::pair<vk::ImageView, vk::ImageView> key = { color_handle ? color_handle->view : nullptr, ds_handle->view };
    auto it = framebuffer_array.find(key);

    if (it != framebuffer_array.end()) {
        // we already created a framebuffer for this pair
        return it->second;
    }

    vk::FramebufferCreateInfo fb_info{
        .renderPass = standard_render_pass,
        .width = width,
        .height = height,
        .layers = 1
    };
    vk::ImageView attachments[] = { color_handle->view, ds_handle->view };
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

    return (framebuffer_array[key] = { fb_standard, fb_interlock });
}

ColorSurfaceCacheInfo *VKSurfaceCache::perform_surface_sync() {
    // surface sync is supported only if memory mapping is enabled
    if (!state.features.support_memory_mapping)
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

    if (state.res_multiplier > 1) {
        // downscale the image using a blit command first

        if (!last_written_surface->blit_image)
            last_written_surface->blit_image = std::make_unique<vkutil::Image>();

        vkutil::Image &blit_image = *last_written_surface->blit_image;

        if (!blit_image.image) {
            blit_image.allocator = state.allocator;
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
            copy_buffer.allocator = state.allocator;
            // TODO: change the 4 if the format pixel size can become something else than 4 bytes (not the case now)
            copy_buffer.size = last_written_surface->pixel_stride * last_written_surface->original_height * 4;
            copy_buffer.init_buffer(vk::BufferUsageFlagBits::eTransferDst, vkutil::vma_mapped_alloc);
        }

        buffer = copy_buffer.buffer;
        offset = 0;
    } else {
        std::tie(buffer, offset) = state.get_matching_mapping(last_written_surface->data);
    }
    vk::BufferImageCopy copy{
        .bufferOffset = offset,
        .bufferRowLength = last_written_surface->pixel_stride,
        .bufferImageHeight = last_written_surface->original_height,
        .imageSubresource = vkutil::color_subresource_layer,
        .imageOffset = { 0, 0, 0 },
        .imageExtent = { last_written_surface->original_width, last_written_surface->original_height, 1 }
    };
    cmd_buffer.copyImageToBuffer(image_to_copy, image_layout, buffer, copy);

    const bool need_post_sync = !is_swizzle_identity || format_need_additional_memory(last_written_surface->format);
    ColorSurfaceCacheInfo *return_value = need_post_sync ? last_written_surface : nullptr;
    last_written_surface = nullptr;

    return return_value;
}

template <typename T>
void swizzle_text_T_2(T *pixels, uint32_t nb_pixel) {
    for (int i = 0; i < nb_pixel; i++) {
        std::swap(pixels[2 * i], pixels[2 * i + 1]);
    }
}

template <typename T, size_t type>
void swizzle_text_T_4(T *pixels, uint32_t nb_pixel) {
    for (int i = 0; i < nb_pixel; i++) {
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
void swizzle_text_T(T *pixels, uint32_t nb_pixel, ColorSurfaceCacheInfo *surface) {
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

    const uint32_t nb_pixels = surface->pixel_stride * surface->original_height;
    uint8_t *pixels = surface->data.cast<uint8_t>().get(mem);

    if (format_need_additional_memory(surface->format)) {
        // special case, use a custom function
        const bool is_swizzle_identity = surface->swizzle.r == vk::ComponentSwizzle::eR;
        if (!surface->sws_context) {
            const AVPixelFormat dst_fmt = is_swizzle_identity ? AV_PIX_FMT_RGB24 : AV_PIX_FMT_BGR24;
            surface->sws_context = sws_getContext(surface->original_width, surface->original_height, AV_PIX_FMT_RGB0, surface->original_width, surface->original_height, dst_fmt, 0, nullptr, nullptr, nullptr);
            assert(surface->sws_context != NULL);
        }

        int src_stride = surface->pixel_stride * 4;
        int dst_stride = surface->pixel_stride * 3;
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
    auto ite = color_surface_textures.upper_bound(address.address());
    if (ite == color_surface_textures.begin()) {
        return nullptr;
    }
    ite--;

    ColorSurfaceCacheInfo &info = ite->second;
    if (info.data.address() + info.total_bytes <= address.address())
        // they do not overlap
        return nullptr;

    if (info.pixel_stride == pitch) {
        // In assumption the format is RGBA8
        const size_t data_delta = address.address() - ite->first;
        uint32_t limited_height = viewport.height;
        if ((data_delta % (pitch * 4)) == 0) {
            uint32_t start_sourced_line = (data_delta / (pitch * 4)) * state.res_multiplier;
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

            if (!info.sampled_image)
                info.sampled_image = std::make_unique<vkutil::Image>();

            if (!info.sampled_image->view) {
                // create a view with the right swizzle and without gamma correction
                vk::ImageViewCreateInfo view_info{
                    .image = info.texture.image,
                    .viewType = vk::ImageViewType::e2D,
                    .format = vk::Format::eR8G8B8A8Unorm,
                    .components = vkutil::color_to_texture_swizzle(info.swizzle, vkutil::rgba_mapping),
                    .subresourceRange = vkutil::color_subresource_range
                };
                info.sampled_image->view = state.device.createImageView(view_info);
            }

            return info.sampled_image->view;
        }
    }

    return nullptr;
}
}; // namespace renderer::vulkan