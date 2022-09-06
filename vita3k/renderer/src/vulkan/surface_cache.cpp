// Vita3K emulator project
// Copyright (C) 2022 Vita3K team
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

#include <util/log.h>

namespace renderer::vulkan {

static constexpr std::uint64_t CASTED_UNUSED_TEXTURE_PURGE_SECS = 40;

void VKSurfaceCache::destroy_framebuffers(vk::ImageView view) {
    for (auto it = framebuffer_array.begin(); it != framebuffer_array.end();) {
        // if the color of depth-stencil match the one of the render_target, this won't be used anymore
        if (it->first.first == view || it->first.second == view) {
            reinterpret_cast<VKContext *>(state.context)->frame().destroy_queue.add(it->second);
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

    // make sure it is not destroyed twice (if set info.sampled_image.image is the same as info.texture.image)
    info.sampled_image.image = nullptr;
    destroy_queue.add_image(info.sampled_image);
    destroy_framebuffers(info.texture.view);
    destroy_queue.add_image(info.texture);
}

void VKSurfaceCache::destroy_surface(DepthStencilSurfaceCacheInfo &info) {
    VKContext *context = reinterpret_cast<VKContext *>(state.context);
    vkutil::DestroyQueue &destroy_queue = context->frame().destroy_queue;

    destroy_queue.add_image(info.read_only);
    destroy_framebuffers(info.texture.view);
    destroy_queue.add_image(info.texture);
}

VKSurfaceCache::VKSurfaceCache(VKState &state)
    : state(state) {
    for (int i = 0; i < MAX_CACHE_SIZE_PER_CONTAINER; i++) {
        depth_stencil_textures[i].flags = SurfaceCacheInfo::FLAG_FREE;
    }
}

vkutil::Image *VKSurfaceCache::retrieve_color_surface_texture_handle(uint16_t width, uint16_t height, const uint16_t pixel_stride,
    const SceGxmColorBaseFormat base_format, const bool is_srgb, Ptr<void> address, SurfaceTextureRetrievePurpose purpose, vk::ComponentMapping &swizzle,
    uint16_t *stored_height, uint16_t *stored_width) {
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
            static bool has_happened = false;
            LOG_WARN_IF(!has_happened, "Trying to use gamma correction with non-compatible format {}", vk::to_string(vk_format));
            has_happened = true;
        }
    }

    uint32_t bytes_per_stride = pixel_stride * gxm::bits_per_pixel(base_format) / 8;
    uint32_t total_surface_size = bytes_per_stride * original_height;

    if (overlap) {
        ColorSurfaceCacheInfo &info = ite->second;
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
        const bool surface_extent_changed = (info.width < width) || (info.height < height);
        bool surface_stat_changed = false;

        if (ite->first == key) {
            if (purpose == SurfaceTextureRetrievePurpose::WRITING) {
                surface_stat_changed = surface_extent_changed || (base_format != info.format);
            } else {
                // If the extent changed but format is not the same, then the probability of it being a cast is high
                surface_stat_changed = surface_extent_changed && (base_format == info.format);
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

                if (!castable) {
                    static bool has_happened = false;
                    LOG_ERROR_IF(!has_happened, "Two surface formats requested=0x{:X} and inStore=0x{:X} are not castable!", base_format, info.format);
                    has_happened = true;
                    return nullptr;
                }
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

                if (start_x > 0) {
                    static bool has_happened = false;
                    LOG_ERROR_IF(!has_happened, "Surface copy with nonzero delta x");
                    has_happened = true;
                }

                const vk::Image color_handle = reinterpret_cast<VKContext *>(state.context)->current_color_attachment->image;
                // TODO: we can read an srgb image as linear (or the opposite) without having to do a copy
                if (info.texture.image == color_handle || (start_sourced_line != 0) || (start_x != 0) || (info.width != width) || (info.height != height) || (info.format != base_format)) {
                    uint64_t current_time = std::chrono::duration_cast<std::chrono::seconds>(
                        std::chrono::steady_clock::now().time_since_epoch())
                                                .count();

                    const uint64_t scene_timestamp = reinterpret_cast<VKContext *>(state.context)->scene_timestamp;

                    std::vector<CastedTexture> &casted_vec = info.casted_textures;

                    vk::Format source_format = color::translate_format(info.format);

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
                    bool is_offscene = false;
                    if (!cmd_buffer) {
                        is_offscene = true;
                        cmd_buffer = vkutil::create_single_time_command(state.device, state.general_command_pool);
                    }

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
                        static bool has_happened = false;
                        LOG_INFO_IF(!has_happened, "Game is doing typeless copies");
                        has_happened = true;
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

                    if (is_offscene)
                        vkutil::end_single_time_command(state.device, state.general_queue, state.general_command_pool, cmd_buffer);

                    return &casted->texture;
                } else {
                    // we must insert a barrier before reading from the texture
                    VKContext *context = reinterpret_cast<VKContext *>(state.context);
                    vk::CommandBuffer cmd_buffer = context->prerender_cmd;
                    vk::ImageMemoryBarrier barrier{
                        .srcAccessMask = vk::AccessFlagBits::eColorAttachmentWrite,
                        .dstAccessMask = vk::AccessFlagBits::eShaderRead,
                        .oldLayout = vk::ImageLayout::eGeneral,
                        .newLayout = vk::ImageLayout::eGeneral,
                        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                        .image = info.texture.image,
                        .subresourceRange = vkutil::color_subresource_range
                    };
                    cmd_buffer.pipelineBarrier(vk::PipelineStageFlagBits::eColorAttachmentOutput, vk::PipelineStageFlagBits::eFragmentShader, vk::DependencyFlags(), {}, {}, barrier);

                    if (swizzle == info.swizzle && vk_format == info.texture.format)
                        // we can use the same texture view
                        return &info.texture;

                    if (!info.sampled_image.view) {
                        vk::ComponentMapping resulting_mapping = vkutil::color_to_texture_swizzle(info.swizzle, swizzle);
                        // do not destroy multiple times
                        info.sampled_image.destroy_on_deletion = false;
                        vk::ImageViewCreateInfo view_info{
                            .image = info.texture.image,
                            .viewType = vk::ImageViewType::e2D,
                            .format = vk_format,
                            .components = resulting_mapping,
                            .subresourceRange = vkutil::color_subresource_range
                        };
                        info.sampled_image.view = state.device.createImageView(view_info);
                    }

                    return &info.sampled_image;
                }
            }
        }

        if (!invalidated) {
            if (purpose == SurfaceTextureRetrievePurpose::WRITING) {
                if (used_iterator != last_use_color_surface_index.end()) {
                    last_use_color_surface_index.erase(used_iterator);
                }

                last_use_color_surface_index.push_back(ite->first);

                if (vk_format == info.texture.format) {
                    return &info.texture;
                } else {
                    // using both srgb/linear
                    if (!info.sampled_image.view) {
                        info.sampled_image.destroy_on_deletion = false;
                        vk::ImageViewCreateInfo view_info{
                            .image = info.texture.image,
                            .viewType = vk::ImageViewType::e2D,
                            .format = vk_format,
                            .components = vkutil::default_comp_mapping,
                            .subresourceRange = vkutil::color_subresource_range
                        };
                        info.sampled_image.view = state.device.createImageView(view_info);
                    }
                    // needed in order not to sample from this image during rendering
                    info.sampled_image.image = info.texture.image;

                    return &info.sampled_image;
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
    // Todo: add rgba8/rgba8srgb as the possible formats in pNext
    vk::ImageCreateFlags image_create_flags = (vk_format == vk::Format::eR8G8B8A8Unorm || is_srgb) ? vk::ImageCreateFlagBits::eMutableFormat : vk::ImageCreateFlags();
    image.init_image(vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eInputAttachment, vkutil::default_comp_mapping, image_create_flags);

    // do it in the prerender if we read from this texture in the same scene (although this would be useless)
    vk::CommandBuffer cmd_buffer = context->prerender_cmd;
    // must do a first transition to draw the placeholder color
    image.transition_to(cmd_buffer, vkutil::ImageLayout::TransferDst);

    vk::ClearColorValue clear_color{ std::array<float, 4>({ 0.968627450f, 0.776470588f, 0.0f, 0.0f }) };
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

    return &info_added.texture;
}

vkutil::Image *VKSurfaceCache::retrieve_depth_stencil_texture_handle(const MemState &mem, const SceGxmDepthStencilSurface &surface, int32_t force_width,
    int32_t force_height, const bool is_reading) {
    bool packed_ds = (surface.control.content & SceGxmDepthStencilControl::format_bits) == SCE_GXM_DEPTH_STENCIL_FORMAT_S8D24;

    if (force_width < 0) {
        force_width = target->width;
    }

    if (force_height < 0) {
        force_height = target->height;
    }

    size_t found_index = -1;

    // The whole depth stencil struct is reserved for future use
    for (size_t i = 0; i < depth_stencil_textures.size(); i++) {
        if ((depth_stencil_textures[i].surface.depthData == surface.depthData) && (packed_ds || (!packed_ds && (depth_stencil_textures[i].surface.stencilData == surface.stencilData)))) {
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
        if (cached_info.width < force_width) {
            if (is_reading)
                return nullptr;
            cached_info.width = force_width;
            need_remake = true;
        }

        if (cached_info.height < force_height) {
            if (is_reading)
                return nullptr;
            cached_info.height = force_height;
            need_remake = true;
        }

        if (!need_remake) {
            if (!is_reading)
                return &cached_info.texture;

            const uint64_t scene_timestamp = reinterpret_cast<VKContext *>(state.context)->scene_timestamp;

            // copy the depth stencil only once per scene
            if (cached_info.scene_timestamp == scene_timestamp)
                return &cached_info.read_only;

            cached_info.scene_timestamp = scene_timestamp;

            // use prerender cmd as we can't copy an image or use pipeline barriers in a render pass
            VKContext *context = reinterpret_cast<VKContext *>(state.context);
            vk::CommandBuffer cmd_buffer = context->prerender_cmd;
            bool is_offscene = false;
            if (!cmd_buffer) {
                is_offscene = true;
                cmd_buffer = vkutil::create_single_time_command(state.device, state.general_command_pool);
            }
            if (!cached_info.read_only.image) {
                vk::ImageSubresourceRange range = vkutil::ds_subresource_range;
                range.aspectMask = vk::ImageAspectFlagBits::eDepth;
                cached_info.read_only.allocator = state.allocator;
                cached_info.read_only.width = cached_info.width;
                cached_info.read_only.height = cached_info.height;
                cached_info.read_only.format = vk::Format::eD32SfloatS8Uint;
                cached_info.read_only.init_image(vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst);
                // we want a texture view with only the depth aspect bit
                // TODO: not efficient
                state.device.destroy(cached_info.read_only.view);
                vk::ImageViewCreateInfo view_info{
                    .image = cached_info.read_only.image,
                    .viewType = vk::ImageViewType::e2D,
                    .format = vk::Format::eD32SfloatS8Uint,
                    .components = {},
                    .subresourceRange = range
                };
                cached_info.read_only.view = state.device.createImageView(view_info);

                cached_info.read_only.transition_to(cmd_buffer, vkutil::ImageLayout::TransferDst, vkutil::ds_subresource_range);
            } else {
                cached_info.read_only.transition_to_discard(cmd_buffer, vkutil::ImageLayout::TransferDst, vkutil::ds_subresource_range);
            }

            cached_info.texture.transition_to(cmd_buffer, vkutil::ImageLayout::TransferSrc, vkutil::ds_subresource_range);
            vk::ImageSubresourceLayers layers = vkutil::color_subresource_layer;
            layers.aspectMask = vk::ImageAspectFlagBits::eDepth | vk::ImageAspectFlagBits::eStencil;
            vk::ImageCopy image_copy{
                .srcSubresource = layers,
                .srcOffset = { 0, 0, 0 },
                .dstSubresource = layers,
                .dstOffset = { 0, 0, 0 },
                .extent = { static_cast<uint32_t>(cached_info.width), static_cast<uint32_t>(cached_info.height), 1U }
            };
            cmd_buffer.copyImage(cached_info.texture.image, vk::ImageLayout::eTransferSrcOptimal, cached_info.read_only.image, vk::ImageLayout::eTransferDstOptimal, image_copy);

            // transition back
            cached_info.texture.transition_to(cmd_buffer, vkutil::ImageLayout::DepthStencilAttachment, vkutil::ds_subresource_range);
            cached_info.read_only.transition_to(cmd_buffer, vkutil::ImageLayout::DepthReadOnly, vkutil::ds_subresource_range);

            if (is_offscene)
                vkutil::end_single_time_command(state.device, state.general_queue, state.general_command_pool, cmd_buffer);

            return &cached_info.read_only;
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
    depth_stencil_textures[found_index].flags = 0;
    depth_stencil_textures[found_index].surface = surface;
    depth_stencil_textures[found_index].width = force_width;
    depth_stencil_textures[found_index].height = force_height;

    vkutil::Image &image = depth_stencil_textures[found_index].texture;

    // use prerender cmd in case we read from the depth buffer (although I really doubt this could happen)
    VKContext *context = reinterpret_cast<VKContext *>(state.context);
    vk::CommandBuffer cmd_buffer = context->prerender_cmd;

    image.allocator = state.allocator;
    image.width = force_width;
    image.height = force_height;
    image.format = vk::Format::eD32SfloatS8Uint;
    image.layout = vkutil::ImageLayout::Undefined;
    image.init_image(vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eTransferSrc);

    image.transition_to(cmd_buffer, vkutil::ImageLayout::TransferDst, vkutil::ds_subresource_range);
    vk::ClearDepthStencilValue clear_value{
        .depth = 1.0,
        .stencil = 0
    };
    cmd_buffer.clearDepthStencilImage(image.image, vk::ImageLayout::eTransferDstOptimal, clear_value, vkutil::ds_subresource_range);
    image.transition_to(cmd_buffer, vkutil::ImageLayout::DepthStencilAttachment, vkutil::ds_subresource_range);

    return &image;
}

vk::Framebuffer VKSurfaceCache::retrieve_framebuffer_handle(const MemState &mem, SceGxmColorSurface *color, SceGxmDepthStencilSurface *depth_stencil,
    vk::RenderPass render_pass, vkutil::Image **color_texture_handle, vkutil::Image **ds_texture_handle,
    uint16_t *stored_height) {
    if (!target) {
        LOG_ERROR("Unable to retrieve framebuffer with no active render target!");
        return {};
    }

    if (!color && !depth_stencil) {
        LOG_ERROR("Depth stencil and color surface are both null!");
        return {};
    }

    // First retrieve separately the color surface and ds surface
    vkutil::Image *color_handle;
    vkutil::Image *ds_handle;

    if (color && color->data) {
        const SceGxmColorBaseFormat color_base_format = gxm::get_base_format(color->colorFormat);
        vk::ComponentMapping swizzle = color::translate_swizzle(color->colorFormat);
        color_handle = retrieve_color_surface_texture_handle(color->width,
            color->height, color->strideInPixels, color_base_format, static_cast<bool>(color->gamma),
            color->data, renderer::SurfaceTextureRetrievePurpose::WRITING, swizzle, stored_height);
    } else {
        color_handle = &target->color;
    }

    if (depth_stencil && (depth_stencil->depthData || depth_stencil->stencilData)) {
        SceGxmDepthStencilSurface surface_copy = *depth_stencil;
        ds_handle = retrieve_depth_stencil_texture_handle(mem, *depth_stencil);
    } else {
        ds_handle = &target->depthstencil;
    }

    if (color_texture_handle) {
        *color_texture_handle = color_handle;
    }

    if (ds_texture_handle) {
        *ds_texture_handle = ds_handle;
    }

    std::pair<vk::ImageView, vk::ImageView> key = { color_handle->view, ds_handle->view };
    auto it = framebuffer_array.find(key);

    if (it != framebuffer_array.end()) {
        // we already created a framebuffer for this pair

        return it->second;
    }

    vk::FramebufferCreateInfo fb_info{
        .renderPass = render_pass,
        .width = target->width,
        .height = target->height,
        .layers = 1
    };
    vk::ImageView attachments[] = { color_handle->view, ds_handle->view };
    fb_info.setAttachments(attachments);
    vk::Framebuffer fb = state.device.createFramebuffer(fb_info);

    framebuffer_array[key] = fb;
    return fb;
}

void VKSurfaceCache::destroy_associated_framebuffers(const VKRenderTarget *render_target) {
    destroy_framebuffers(render_target->color.view);
    destroy_framebuffers(render_target->depthstencil.view);
}

vk::ImageView VKSurfaceCache::sourcing_color_surface_for_presentation(Ptr<const void> address, uint32_t width, uint32_t height, const std::uint32_t pitch, std::array<float, 4> &uvs, const int res_multiplier, SceFVector2 &texture_size) {
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

    width *= res_multiplier;
    height *= res_multiplier;

    if (info.pixel_stride == pitch) {
        // In assumption the format is RGBA8
        const std::size_t data_delta = address.address() - ite->first;
        std::uint32_t limited_height = height;
        if ((data_delta % (pitch * 4)) == 0) {
            std::uint32_t start_sourced_line = (data_delta / (pitch * 4)) * res_multiplier;
            if ((start_sourced_line + height) > info.height) {
                // Sometimes the surface is just missing a little bit of lines
                if (start_sourced_line < info.height) {
                    // Just limit the height and display it
                    limited_height = info.height - start_sourced_line;
                } else {
                    LOG_ERROR("Trying to present non-existent segment in cached color surface!");
                    return nullptr;
                }
            }

            // Calculate uvs
            // First two top left, the two others bottom right
            uvs[0] = 0.0f;
            uvs[1] = static_cast<float>(start_sourced_line) / info.height;
            uvs[2] = static_cast<float>(width) / info.width;
            uvs[3] = static_cast<float>(start_sourced_line + limited_height) / info.height;

            texture_size.x = info.width;
            texture_size.y = info.height;

            if (info.swizzle == vkutil::rgba_mapping && info.texture.format == vk::Format::eR8G8B8A8Unorm)
                return info.texture.view;

            if (!info.sampled_image.view) {
                // create a view with the right swizzle and without gamma correction
                info.sampled_image.destroy_on_deletion = false;
                vk::ImageViewCreateInfo view_info{
                    .image = info.texture.image,
                    .viewType = vk::ImageViewType::e2D,
                    .format = vk::Format::eR8G8B8A8Unorm,
                    .components = vkutil::color_to_texture_swizzle(info.swizzle, vkutil::rgba_mapping),
                    .subresourceRange = vkutil::color_subresource_range
                };
                info.sampled_image.view = state.device.createImageView(view_info);
            }

            return info.sampled_image.view;
        }
    }

    return nullptr;
}
}; // namespace renderer::vulkan