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

#include <renderer/vulkan/functions.h>

#include <renderer/vulkan/gxm_to_vulkan.h>

#include <vulkan/vulkan_format_traits.hpp>

#include <gxm/functions.h>
#include <gxm/types.h>
#include <renderer/functions.h>
#include <util/align.h>
#include <vkutil/vkutil.h>

namespace renderer::vulkan {

// return if this format can be used to read a depth stencil buffer
// Only return the formats we support and make sense for now
// (technically we can read a D24S8 or D32 as R8R8R8R8, but it is not implemented
// yet and no game I am aware of does it)
static bool is_depth_stencil_compatible_format(SceGxmTextureBaseFormat format) {
    switch (format) {
        // 8bit stencil
    case SCE_GXM_TEXTURE_BASE_FORMAT_U8:
        // D16 format
    case SCE_GXM_TEXTURE_BASE_FORMAT_U16:
        // D32 format
    case SCE_GXM_TEXTURE_BASE_FORMAT_F32:
        // D32M format
    case SCE_GXM_TEXTURE_BASE_FORMAT_F32M:
        // D24S8 format
    case SCE_GXM_TEXTURE_BASE_FORMAT_X8U24:
        return true;
    default:
        return false;
    }
}

VKTextureCache::VKTextureCache(VKState &state)
    : state(state) {}

void sync_texture(VKContext &context, MemState &mem, std::size_t index, SceGxmTexture texture, const Config &config,
    const std::string &base_path, const std::string &title_id) {
    // why are we doing this here?
    // well textures are synced right before the draw
    // in particular, we know that the scissor is the correct one for the upcoming draw
    // and the texture copy needs to be in the correct prerender command for it to render fine
    // so start a new recording right now if the macroblock has changed
    context.check_for_macroblock_change(false);

    Address data_addr = texture.data_addr << 2;
    bool is_vertex = index >= SCE_GXM_MAX_TEXTURE_UNITS;

    const size_t texture_size = renderer::texture::texture_size(texture);

    const SceGxmTextureFormat format = gxm::get_format(&texture);
    const SceGxmTextureBaseFormat base_format = gxm::get_base_format(format);
    if (gxm::is_paletted_format(base_format) && texture.palette_addr == 0) {
        LOG_WARN("Ignoring null palette texture");
        return;
    }

    if (index >= SCE_GXM_MAX_TEXTURE_UNITS) {
        // Vertex textures
        context.shader_hints.vertex_textures[index - SCE_GXM_MAX_TEXTURE_UNITS] = format;
    } else {
        context.shader_hints.fragment_textures[index] = format;
    }

    vkutil::Image *image = nullptr;

    SceGxmColorBaseFormat format_target_of_texture;

    uint16_t width = static_cast<uint16_t>(gxm::get_width(&texture));
    uint16_t height = static_cast<uint16_t>(gxm::get_height(&texture));

    TextureViewport texture_viewport{};

    if (renderer::texture::convert_base_texture_format_to_base_color_format(base_format, format_target_of_texture)) {
        uint16_t stride_in_pixels = width;

        SceGxmColorSurfaceType surface_type = SCE_GXM_COLOR_SURFACE_LINEAR;
        switch (texture.texture_type()) {
        case SCE_GXM_TEXTURE_LINEAR_STRIDED:
            stride_in_pixels = static_cast<uint16_t>(gxm::get_stride_in_bytes(&texture)) / ((renderer::texture::bits_per_pixel(base_format) + 7) >> 3);
            break;
        case SCE_GXM_TEXTURE_LINEAR:
            // when the texture is linear, the stride should be aligned to 8 pixels
            stride_in_pixels = align(stride_in_pixels, 8);
            break;
        case SCE_GXM_TEXTURE_TILED:
            // tiles are 32x32
            stride_in_pixels = align(stride_in_pixels, 32);
            surface_type = SCE_GXM_COLOR_SURFACE_TILED;
            break;
        case SCE_GXM_TEXTURE_SWIZZLED:
        case SCE_GXM_TEXTURE_SWIZZLED_ARBITRARY:
            surface_type = SCE_GXM_COLOR_SURFACE_SWIZZLED;
        }

        vk::ComponentMapping swizzle = texture::translate_swizzle(format);

        image = context.state.surface_cache.retrieve_color_surface_texture_handle(
            mem, width, height, stride_in_pixels, format_target_of_texture, surface_type, static_cast<bool>(texture.gamma_mode), Ptr<void>(data_addr),
            renderer::SurfaceTextureRetrievePurpose::READING, swizzle, nullptr, nullptr, &texture_viewport);
    }

    if (!image && is_depth_stencil_compatible_format(base_format)) {
        // Try to retrieve depth/stencil
        SceGxmDepthStencilSurface lookup_temp;
        lookup_temp.depth_data = data_addr;
        lookup_temp.stencil_data.reset();

        image = context.state.surface_cache.retrieve_depth_stencil_texture_handle(mem, lookup_temp, width, height, true, &texture_viewport);
    }

    if (image) {
        if (!image->sampler)
            image->sampler = texture::create_sampler(context.state, texture);
    } else {
        if (!is_valid_addr_range(mem, data_addr, data_addr + texture_size)) {
            LOG_WARN("Texture has freed data.");
            return;
        }
        context.state.texture_cache.cache_and_bind_texture(texture, mem);
        image = &context.state.texture_cache.current_texture->texture;
    }

    const vk::ImageLayout layout = vkutil::get_underlying_layout(image->layout);

    vk::DescriptorImageInfo &image_info = is_vertex
        ? context.vertex_textures[index - SCE_GXM_MAX_TEXTURE_UNITS]
        : context.fragment_textures[index];
    if (image_info.sampler != image->sampler || image_info.imageView != image->view) {
        image_info = vk::DescriptorImageInfo{
            .sampler = image->sampler,
            .imageView = image->view,
            .imageLayout = layout
        };
        // invalidate last descriptor set
        if (is_vertex)
            context.last_vert_texture_count = ~0;
        else
            context.last_frag_texture_count = ~0;
    }

    if (context.state.features.use_texture_viewport) {
        if (is_vertex) {
            context.curr_vert_ublock.set_viewport_ratio(index - SCE_GXM_MAX_TEXTURE_UNITS, texture_viewport.ratio);
            context.curr_vert_ublock.set_viewport_offset(index - SCE_GXM_MAX_TEXTURE_UNITS, texture_viewport.offset);
        } else {
            context.curr_frag_ublock.set_viewport_ratio(index, texture_viewport.ratio);
            context.curr_frag_ublock.set_viewport_offset(index, texture_viewport.offset);
        }
    }
}

void VKTextureCache::prepare_staging_buffer(bool is_configure) {
    assert(!is_texture_transfer_ready);
    VKContext *context = reinterpret_cast<VKContext *>(state.context);

    TextureStagingBuffer *staging_buffer = &staging_buffers[staging_idx];
    // some textures must be 16-bytes aligned, and staging_buffer->buffer.size is 16-bytes aligned
    staging_buffer->used_so_far = align(staging_buffer->used_so_far, 16);
    // we can keep using the same buffer as before if we are in the same scene and there is enough memory left
    bool use_previous_buffer = (current_scene_timestamp == staging_buffer->scene_timestamp) && (staging_buffer->buffer.size - staging_buffer->used_so_far) >= current_texture->memory_needed;

    if (!use_previous_buffer) {
        staging_idx = (staging_idx + 1) % NB_TEXTURE_STAGING_BUFFERS;
        staging_buffer = &staging_buffers[staging_idx];
    }

    // if we are not using the previous buffer, we wait if the buffer was used at least once,
    // less than MAX_FRAMES_RENDERING frames ago and we have not yet waited for its fence
    const bool need_wait = !use_previous_buffer
        && staging_buffer->frame_timestamp != ~0
        && staging_buffer->frame_timestamp > context->frame_timestamp - MAX_FRAMES_RENDERING
        && staging_buffer->scene_timestamp > last_waited_scene;
    const vk::Fence current_fence = context->next_fence;

    if (need_wait) {
        if (staging_buffer->scene_timestamp == current_scene_timestamp) {
            assert(current_fence == staging_buffer->waiting_fence);
            // special case, all the staging buffer are occupied by the current scene
            // submit the command buffer and wait for it
            context->prerender_cmd.end();
            context->cmdbuffers_to_submit.push_back(context->prerender_cmd);

            vk::SubmitInfo submit_info{};
            submit_info.setCommandBuffers(context->cmdbuffers_to_submit);
            state.general_queue.submit(submit_info, current_fence);
            context->cmdbuffers_to_submit.clear();

            auto result = state.device.waitForFences(current_fence, VK_TRUE, std::numeric_limits<uint64_t>::max());
            if (result != vk::Result::eSuccess) {
                LOG_ERROR("Could not wait for fences.");
                assert(false);
                return;
            }
            state.device.resetFences(current_fence);

            // also call begin again on the prerender command
            vk::CommandBufferBeginInfo begin_info{
                .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit
            };
            context->prerender_cmd.begin(begin_info);

            // then set all buffers as unused
            for (auto &buffer : staging_buffers) {
                buffer.frame_timestamp = ~0;
                buffer.scene_timestamp = ~0;
            }
        } else {
            // wait for the fence, but don't reset it
            auto result = state.device.waitForFences(staging_buffer->waiting_fence, VK_TRUE, std::numeric_limits<uint64_t>::max());
            if (result != vk::Result::eSuccess) {
                LOG_ERROR("Could not wait for fences.");
                assert(false);
                return;
            }
            last_waited_scene = staging_buffer->scene_timestamp;
        }
    }

    // then we can use the buffer
    cmd_buffer = context->prerender_cmd;
    if (!use_previous_buffer) {
        staging_buffer->scene_timestamp = current_scene_timestamp;
        staging_buffer->frame_timestamp = context->frame_timestamp;
        staging_buffer->waiting_fence = current_fence;
        staging_buffer->used_so_far = 0;

        if (staging_buffer->buffer.size < current_texture->memory_needed) {
            // we need to create a bigger buffer
            // destroy the previous one if there is, no need to defer destroy it as we know it is no longer being used
            staging_buffer->buffer.destroy();

            staging_buffer->buffer.size = current_texture->memory_needed;
            staging_buffer->buffer.init_buffer(vk::BufferUsageFlagBits::eTransferSrc, vkutil::vma_mapped_alloc);
        }
    }

    // now the transition
    vk::ImageSubresourceRange range{
        .aspectMask = vk::ImageAspectFlagBits::eColor,
        .baseMipLevel = 0,
        .levelCount = current_texture->mip_count,
        .baseArrayLayer = 0,
        .layerCount = current_texture->is_cube ? 6U : 1U
    };

    // if this is done during configure, layout is undefined, otherwise it is shader read only
    if (is_configure)
        vkutil::transition_image_layout(cmd_buffer, current_texture->texture.image, vkutil::ImageLayout::Undefined, vkutil::ImageLayout::TransferDst, range);
    else
        vkutil::transition_image_layout_discard(cmd_buffer, current_texture->texture.image, vkutil::ImageLayout::SampledImage, vkutil::ImageLayout::TransferDst, range);

    is_texture_transfer_ready = true;
}

bool VKTextureCache::init(const bool hashless_texture_cache) {
    TextureCache::init(hashless_texture_cache);
    backend = Backend::Vulkan;

    // don't forget to specify the allocator for all the staging buffers
    for (int i = 0; i < NB_TEXTURE_STAGING_BUFFERS; i++)
        staging_buffers[i].buffer.allocator = state.allocator;

    return true;
}

void VKTextureCache::select(size_t index, const SceGxmTexture &texture) {
    current_texture = &textures[index];
    is_texture_transfer_ready = false;
}

static vk::Format linear_to_srgb(const vk::Format format) {
    switch (format) {
    case vk::Format::eR8Unorm:
        return vk::Format::eR8Srgb;
    case vk::Format::eR8G8Unorm:
        return vk::Format::eR8G8Srgb;
    case vk::Format::eR8G8B8A8Unorm:
        return vk::Format::eR8G8B8A8Srgb;
    case vk::Format::eBc1RgbaUnormBlock:
        return vk::Format::eBc1RgbaSrgbBlock;
    case vk::Format::eBc2UnormBlock:
        return vk::Format::eBc2SrgbBlock;
    case vk::Format::eBc3UnormBlock:
        return vk::Format::eBc3SrgbBlock;
    default: {
        LOG_WARN_ONCE("Trying to use gamma correction with non-compatible format {}", vk::to_string(format));
        return format;
    }
    }
}

// get an upper bound on the amount of memory needed to upload this texture
// the bound is only for one face and the base mip
static uint32_t get_image_memory_upper_bound(const SceGxmTexture &gxm_texture, const vk::Format vk_format, const SceGxmTextureBaseFormat base_format) {
    // we only want an upper bound, it doesn't matter if because of some conversions we later get the width instead of the stride
    // aligning to 8 takes care of the default stride values
    uint32_t stride = align(gxm::get_width(&gxm_texture), 8);
    uint32_t height = align(gxm::get_height(&gxm_texture), 8);

    if (gxm_texture.texture_type() == SCE_GXM_TEXTURE_LINEAR_STRIDED) {
        const uint32_t bpp = (renderer::texture::bits_per_pixel(base_format) + 7) / 8;
        // the max is to handle the case of P4 textures
        stride = std::max<uint32_t>(stride, gxm::get_stride_in_bytes(&gxm_texture) / bpp);
    }

    return ((stride * height) / vk::texelsPerBlock(vk_format)) * vk::blockSize(vk_format);
}

void VKTextureCache::configure_texture(const SceGxmTexture &gxm_texture) {
    const SceGxmTextureFormat format = gxm::get_format(&gxm_texture);
    const SceGxmTextureBaseFormat base_format = gxm::get_base_format(format);

    const vk::ComponentMapping swizzle = texture::translate_swizzle(format);

    const bool is_cube = (gxm_texture.texture_type() == SCE_GXM_TEXTURE_CUBE || gxm_texture.texture_type() == SCE_GXM_TEXTURE_CUBE_ARBITRARY);

    uint32_t width = static_cast<uint32_t>(gxm::get_width(&gxm_texture));
    uint32_t height = static_cast<uint32_t>(gxm::get_height(&gxm_texture));
    if (gxm::is_block_compressed_format(base_format)) {
        // align width and height to block size
        width = align(width, 4);
        height = align(height, 4);
    }

    const uint16_t mip_count = renderer::texture::get_upload_mip(gxm_texture.true_mip_count(), width, height, base_format);

    vk::Format vk_format = texture::translate_format(base_format);
    if (gxm_texture.gamma_mode) {
        vk_format = linear_to_srgb(vk_format);
    }

    current_texture->mip_count = mip_count;
    current_texture->is_cube = is_cube;
    uint32_t memory_needed = get_image_memory_upper_bound(gxm_texture, vk_format, base_format);
    if (mip_count > 1)
        // using mips, the overall memory needed will be 4/3 of the base memory
        // round up to 3/2
        memory_needed += memory_needed / 2;
    if (is_cube)
        memory_needed *= 6;
    current_texture->memory_needed = align(memory_needed, 16);
    vkutil::Image &image = current_texture->texture;

    // In case the cache is full, no need to put the previous image in the destroy queue
    // as it shouldn't have been used for a while
    image.destroy();

    // manually initialize the image
    image.allocator = state.allocator;
    image.width = width;
    image.height = height;
    image.format = vk_format;

    // create image
    vk::ImageCreateInfo image_info{
        .flags = is_cube ? vk::ImageCreateFlagBits::eCubeCompatible : vk::ImageCreateFlags(),
        .imageType = vk::ImageType::e2D,
        .format = vk_format,
        .extent = vk::Extent3D{
            .width = width,
            .height = height,
            .depth = 1 },
        .mipLevels = mip_count,
        .arrayLayers = is_cube ? 6U : 1U,
        .samples = vk::SampleCountFlagBits::e1,
        .tiling = vk::ImageTiling::eOptimal,
        .usage = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst,
        .sharingMode = vk::SharingMode::eExclusive,
        .initialLayout = vk::ImageLayout::eUndefined,
    };

    std::tie(image.image, image.allocation) = image.allocator.createImage(image_info, vkutil::vma_auto_alloc);

    // create image view
    vk::ImageSubresourceRange range{
        .aspectMask = vk::ImageAspectFlagBits::eColor,
        .baseMipLevel = 0,
        .levelCount = mip_count,
        .baseArrayLayer = 0,
        .layerCount = is_cube ? 6U : 1U
    };

    vk::ImageViewCreateInfo view_info{
        .image = image.image,
        .viewType = is_cube ? vk::ImageViewType::eCube : vk::ImageViewType::e2D,
        .format = vk_format,
        .components = swizzle,
        .subresourceRange = range
    };
    image.view = state.device.createImageView(view_info);

    image.sampler = texture::create_sampler(state, gxm_texture, mip_count);

    if (!gxm_texture.normalize_mode)
        LOG_ERROR("Unhandled unnormalized texture, please report it to the developers");

    prepare_staging_buffer(true);
}

// add an alpha channel to u8u8u8 textures
static void *add_alpha_channel(const void *pixels, const uint32_t width, const uint32_t height, std::vector<uint8_t> &data) {
    data.resize(width * height * 4);

    const uint8_t *src = reinterpret_cast<const uint8_t *>(pixels);
    uint8_t *dst = data.data();
    for (uint32_t y = 0; y < height; y++) {
        for (uint32_t x = 0; x < width; x++) {
            dst[0] = src[0];
            dst[1] = src[1];
            dst[2] = src[2];
            // set 1.0 as the alpha channel
            dst[3] = 255;

            src += 3;
            dst += 4;
        }
    }

    return data.data();
}

void VKTextureCache::upload_texture_impl(SceGxmTextureBaseFormat base_format, uint32_t width, uint32_t height,
    uint32_t mip_index, const void *pixels, int face, bool is_compressed, size_t pixels_per_stride) {
    if (!is_texture_transfer_ready)
        prepare_staging_buffer();

    vkutil::Image &image = current_texture->texture;
    TextureStagingBuffer &staging_buffer = staging_buffers[staging_idx];

    if (face > 0)
        face--;

    if (pixels_per_stride == 0)
        pixels_per_stride = width;

    const void *text_data = pixels;
    std::vector<uint8_t> temp_data;
    if (base_format == SCE_GXM_TEXTURE_BASE_FORMAT_U8U8U8 || base_format == SCE_GXM_TEXTURE_BASE_FORMAT_S8S8S8) {
        text_data = add_alpha_channel(pixels, pixels_per_stride, height, temp_data);
        base_format = (base_format == SCE_GXM_TEXTURE_BASE_FORMAT_U8U8U8) ? SCE_GXM_TEXTURE_BASE_FORMAT_U8U8U8U8 : SCE_GXM_TEXTURE_BASE_FORMAT_S8S8S8S8;
    }

    vk::DeviceSize upload_size;
    if (is_compressed) {
        upload_size = renderer::texture::get_compressed_size(base_format, width, height);
    } else {
        size_t bpp = renderer::texture::bits_per_pixel(base_format);
        size_t bytes_per_pixel = (bpp + 7) >> 3;
        upload_size = pixels_per_stride * height * bytes_per_pixel;
    }

    if (staging_buffer.used_so_far + upload_size > staging_buffer.buffer.size) {
        LOG_ERROR("Staging buffer size left ({}) is too small for texture size {}!", staging_buffer.buffer.size - staging_buffer.used_so_far, upload_size);
        return;
    }

    memcpy(reinterpret_cast<uint8_t *>(staging_buffer.buffer.mapped_data) + staging_buffer.used_so_far, text_data, upload_size);

    vk::ImageSubresourceLayers layer{
        .aspectMask = vk::ImageAspectFlagBits::eColor,
        .mipLevel = mip_index,
        .baseArrayLayer = static_cast<uint32_t>(face),
        .layerCount = 1
    };
    vk::BufferImageCopy region{
        .bufferOffset = staging_buffer.used_so_far,
        .bufferRowLength = static_cast<uint32_t>(pixels_per_stride),
        .bufferImageHeight = height,
        .imageSubresource = layer,
        .imageOffset = { 0, 0, 0 },
        .imageExtent = { width, height, 1 }
    };
    cmd_buffer.copyBufferToImage(staging_buffer.buffer.buffer, image.image, vk::ImageLayout::eTransferDstOptimal, region);
    staging_buffer.used_so_far += upload_size;
}

void VKTextureCache::upload_done() {
    // transition the texture back to read only
    vk::ImageSubresourceRange range{
        .aspectMask = vk::ImageAspectFlagBits::eColor,
        .baseMipLevel = 0,
        .levelCount = current_texture->mip_count,
        .baseArrayLayer = 0,
        .layerCount = current_texture->is_cube ? 6U : 1U
    };
    vkutil::transition_image_layout(cmd_buffer, current_texture->texture.image, vkutil::ImageLayout::TransferDst, vkutil::ImageLayout::SampledImage, range);
    current_texture->texture.layout = vkutil::ImageLayout::SampledImage;
    // this should not be necessary
    cmd_buffer = nullptr;
    is_texture_transfer_ready = false;
}

namespace texture {

vk::Sampler create_sampler(VKState &state, const SceGxmTexture &gxm_texture, const uint16_t mip_count) {
    // linear strided textures use the mag filter as the min filter too
    const bool is_linear_strided = gxm_texture.texture_type() == SCE_GXM_TEXTURE_LINEAR_STRIDED;

    const SceGxmTextureAddrMode uaddr = static_cast<SceGxmTextureAddrMode>(gxm_texture.uaddr_mode);
    const SceGxmTextureAddrMode vaddr = static_cast<SceGxmTextureAddrMode>(gxm_texture.vaddr_mode);
    // Note: I don't know what to do with the MIPMAP version of SceGxmTextureFilter
    const SceGxmTextureFilter mag_filter = static_cast<SceGxmTextureFilter>(gxm_texture.mag_filter);
    const SceGxmTextureFilter min_filter = is_linear_strided ? mag_filter : static_cast<SceGxmTextureFilter>(gxm_texture.min_filter);

    // create sampler
    vk::SamplerCreateInfo sampler_info{
        .magFilter = translate_filter(mag_filter),
        .minFilter = translate_filter(min_filter),
        .mipmapMode = gxm_texture.mip_filter ? vk::SamplerMipmapMode::eLinear : vk::SamplerMipmapMode::eNearest,
        .addressModeU = translate_address_mode(uaddr),
        .addressModeV = translate_address_mode(vaddr),
        .addressModeW = vk::SamplerAddressMode::eRepeat,
        .mipLodBias = (static_cast<float>(gxm_texture.lod_bias) - 31.f) / 8.f,
        .maxAnisotropy = static_cast<float>(state.texture_cache.anisotropic_filtering),
        .compareEnable = VK_FALSE,
        .minLod = (mip_count > 1) ? static_cast<float>(std::min<uint16_t>(mip_count, gxm_texture.lod_min0 | (gxm_texture.lod_min1 << 2))) : 0.f,
        // https://registry.khronos.org/vulkan/specs/1.3-extensions/man/html/VkSamplerCreateInfo.html
        // if there is no mipmap, set maxLod to 0.25 so it uses both the magnification or minification filter when needed
        .maxLod = (mip_count > 1) ? static_cast<float>(mip_count) : 0.25f,
        .unnormalizedCoordinates = VK_FALSE,
    };

    // when using nearest filter, disable anisotropy as the pixels can contain data other than color
    sampler_info.anisotropyEnable = (state.texture_cache.anisotropic_filtering > 1) && (sampler_info.magFilter != vk::Filter::eNearest || sampler_info.minFilter != vk::Filter::eNearest);

    return state.device.createSampler(sampler_info);
}
} // namespace texture

} // namespace renderer::vulkan
