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

#include <renderer/vulkan/functions.h>

#include <renderer/vulkan/gxm_to_vulkan.h>

#include <gxm/functions.h>
#include <renderer/functions.h>
#include <util/align.h>
#include <vkutil/vkutil.h>

namespace renderer::vulkan {
VKTextureCacheState::VKTextureCacheState(VKState &state)
    : state(state) {}

void sync_texture(VKContext &context, MemState &mem, std::size_t index, SceGxmTexture texture, const Config &config,
    const std::string &base_path, const std::string &title_id) {
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
    vk::ComponentMapping swizzle_surface;
    bool only_nearest = false;

    SceGxmColorBaseFormat format_target_of_texture;

    uint16_t width = static_cast<std::uint16_t>(gxm::get_width(&texture));
    uint16_t height = static_cast<std::uint16_t>(gxm::get_height(&texture));

    if (renderer::texture::convert_base_texture_format_to_base_color_format(base_format, format_target_of_texture)) {
        std::uint16_t stride_in_pixels = width;

        switch (texture.texture_type()) {
        case SCE_GXM_TEXTURE_LINEAR_STRIDED:
            stride_in_pixels = static_cast<std::uint16_t>(gxm::get_stride_in_bytes(&texture)) / ((renderer::texture::bits_per_pixel(base_format) + 7) >> 3);
            break;
        case SCE_GXM_TEXTURE_LINEAR:
            // when the texture is linear, the stride should be aligned to 8 pixels
            stride_in_pixels = align(stride_in_pixels, 8);
            break;
        case SCE_GXM_TEXTURE_TILED:
            // tiles are 32x32
            stride_in_pixels = align(stride_in_pixels, 32);
            break;
        }

        vk::ComponentMapping swizzle = texture::translate_swizzle(format);

        image = context.state.surface_cache.retrieve_color_surface_texture_handle(
            width, height, stride_in_pixels, format_target_of_texture, static_cast<bool>(texture.gamma_mode), Ptr<void>(data_addr),
            renderer::SurfaceTextureRetrievePurpose::READING, swizzle);
    }

    vk::ImageLayout layout = vk::ImageLayout::eShaderReadOnlyOptimal;
    if (image) {
        layout = vk::ImageLayout::eGeneral;
    } else {
        // Try to retrieve S24D8 texture
        SceGxmDepthStencilSurface lookup_temp;
        lookup_temp.depthData = data_addr;
        lookup_temp.stencilData.reset();

        image = context.state.surface_cache.retrieve_depth_stencil_texture_handle(mem, lookup_temp, width, height, true);
    }

    if (image) {
        if (!image->sampler)
            image->sampler = texture::create_sampler(context.state, texture);
    } else {
        if (!is_valid_addr_range(mem, data_addr, data_addr + texture_size)) {
            LOG_WARN("Texture has freed data.");
            return;
        }
        renderer::texture::cache_and_bind_texture(context.state.texture_cache, texture, mem);
        image = context.state.texture_cache.current_texture;
    }

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
}

namespace texture {

bool init(VKTextureCacheState &cache, const bool hashless_texture_cache) {
    cache.select_callback = [&cache](const std::size_t index, const void *texture) {
        cache.current_texture = &cache.textures[index];
    };

    cache.configure_texture_callback = [](const renderer::TextureCacheState &text_cache, const void *texture) {
        configure_bound_texture(text_cache, *reinterpret_cast<const SceGxmTexture *>(texture));
    };

    cache.upload_texture_callback = [&cache](SceGxmTextureBaseFormat base_format, uint32_t width, uint32_t height, uint32_t mip_index, const void *pixels, int face, bool is_compressed, size_t pixels_per_stride) {
        upload_bound_texture(cache, base_format, width, height, mip_index, pixels, face, is_compressed, pixels_per_stride);
    };

    cache.use_protect = hashless_texture_cache;

    vk::FenceCreateInfo fence_info{};
    cache.wait_fence = cache.state.device.createFence(fence_info);

    return true;
}

static uint32_t max_mip(uint32_t pixels) {
    return std::bit_width(pixels);
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
        static bool has_happened = false;
        LOG_WARN_IF(!has_happened, "Trying to use gamma correction with non-compatible format {}", vk::to_string(format));
        has_happened = true;
        return format;
    }
    }
}

void configure_bound_texture(const renderer::TextureCacheState &state, const SceGxmTexture &gxm_texture) {
    const SceGxmTextureFormat format = gxm::get_format(&gxm_texture);
    const SceGxmTextureBaseFormat base_format = gxm::get_base_format(format);

    const bool mipmap_enabled = static_cast<bool>(gxm_texture.mip_filter);

    const vk::ComponentMapping swizzle = translate_swizzle(format);

    const bool is_cube = (gxm_texture.texture_type() == SCE_GXM_TEXTURE_CUBE || gxm_texture.texture_type() == SCE_GXM_TEXTURE_CUBE_ARBITRARY);

    uint32_t width = static_cast<uint32_t>(gxm::get_width(&gxm_texture));
    uint32_t height = static_cast<uint32_t>(gxm::get_height(&gxm_texture));
    if (gxm::is_block_compressed_format(base_format)) {
        // align width and height to block size
        width = align(width, 4);
        height = align(height, 4);
    }

    uint16_t mip_count = renderer::texture::get_upload_mip(gxm_texture.true_mip_count(), width, height, base_format);
    if (gxm_texture.texture_type() == SCE_GXM_TEXTURE_LINEAR_STRIDED) {
        mip_count = 1;
    }

    vk::Format vk_format = translate_format(base_format);
    if (gxm_texture.gamma_mode) {
        vk_format = linear_to_srgb(vk_format);
    }

    const auto texture_type = gxm_texture.texture_type();
    const bool is_swizzled = (texture_type == SCE_GXM_TEXTURE_SWIZZLED) || (texture_type == SCE_GXM_TEXTURE_CUBE) || (texture_type == SCE_GXM_TEXTURE_SWIZZLED_ARBITRARY) || (texture_type == SCE_GXM_TEXTURE_CUBE_ARBITRARY);
    const auto base_fmt = gxm::get_base_format(format);

    const VKTextureCacheState &cache = static_cast<const VKTextureCacheState &>(state);
    vkutil::Image &image = *cache.current_texture;

    // manually initialize the image
    image.allocator = cache.state.allocator;
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
    image.view = cache.state.device.createImageView(view_info);

    image.sampler = create_sampler(cache.state, gxm_texture, mip_count);
}

vk::Sampler create_sampler(VKState &state, const SceGxmTexture &gxm_texture, const uint16_t mip_count) {
    const SceGxmTextureAddrMode uaddr = static_cast<SceGxmTextureAddrMode>(gxm_texture.uaddr_mode);
    const SceGxmTextureAddrMode vaddr = static_cast<SceGxmTextureAddrMode>(gxm_texture.vaddr_mode);
    const SceGxmTextureFilter min_filter = static_cast<SceGxmTextureFilter>(gxm_texture.min_filter);
    const SceGxmTextureFilter mag_filter = static_cast<SceGxmTextureFilter>(gxm_texture.mag_filter);
    const bool mipmap_enabled = static_cast<bool>(gxm_texture.mip_filter);

    // create sampler
    vk::SamplerCreateInfo sampler_info{
        .magFilter = translate_filter(mag_filter),
        .minFilter = translate_filter(min_filter),
        .mipmapMode = translate_mimpmap_mode(min_filter),
        .addressModeU = translate_address_mode(uaddr),
        .addressModeV = translate_address_mode(vaddr),
        .addressModeW = vk::SamplerAddressMode::eRepeat,
        .mipLodBias = (static_cast<float>(gxm_texture.lod_bias) - 31.f) / 8.f,
        .maxAnisotropy = static_cast<float>(state.texture_cache.anisotropic_filtering),
        .compareEnable = VK_FALSE,
        .minLod = mipmap_enabled ? static_cast<float>(std::min<uint16_t>(mip_count, gxm_texture.lod_min0 | (gxm_texture.lod_min1 << 2))) : 0.f,
        // https://registry.khronos.org/vulkan/specs/1.3-extensions/man/html/VkSamplerCreateInfo.html
        // if there is no mipmap, set maxLod to 0.25 so it uses both the magnification or minification filter when needed
        .maxLod = mipmap_enabled ? static_cast<float>(mip_count) : 0.25f,
        .unnormalizedCoordinates = VK_FALSE,
    };

    // when using nearest filter, disable anisotropy as the pixels can contain data other than color
    sampler_info.anisotropyEnable = (state.texture_cache.anisotropic_filtering > 1) && (sampler_info.magFilter != vk::Filter::eNearest || sampler_info.minFilter != vk::Filter::eNearest);

    return state.device.createSampler(sampler_info);
}

// add an alpha channel to u8u8u8 textures
static void *add_alpha_channel(const void *pixels, const uint32_t width, const uint32_t height, std::vector<uint8_t> &data) {
    data.resize(width * height * 4);

    const uint8_t *src = reinterpret_cast<const uint8_t *>(pixels);
    uint8_t *dst = data.data();
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
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

void upload_bound_texture(VKTextureCacheState &cache, SceGxmTextureBaseFormat base_format, uint32_t width, uint32_t height,
    uint32_t mip_index, const void *pixels, int face, bool is_compressed, size_t pixels_per_stride) {
    vkutil::Image &image = *cache.current_texture;

    const void *text_data = pixels;
    std::vector<uint8_t> temp_data;
    if (base_format == SCE_GXM_TEXTURE_BASE_FORMAT_U8U8U8) {
        text_data = add_alpha_channel(pixels, width, height, temp_data);
        base_format = SCE_GXM_TEXTURE_BASE_FORMAT_U8U8U8U8;
    }

    if (face > 0)
        face--;

    if (pixels_per_stride == 0)
        pixels_per_stride = width;

    vk::DeviceSize upload_size;
    if (is_compressed) {
        upload_size = renderer::texture::get_compressed_size(base_format, width, height);
    } else {
        size_t bpp = renderer::texture::bits_per_pixel(base_format);
        size_t bytes_per_pixel = (bpp + 7) >> 3;
        upload_size = pixels_per_stride * height * bytes_per_pixel;
    }

    if (cache.alloc_info.size < upload_size) {
        // we need to create a bigger buffer
        if (cache.staging_buffer) {
            // delete the previous one if there is
            cache.state.allocator.destroyBuffer(cache.staging_buffer, cache.alloc);
        }

        vk::BufferCreateInfo buffer_info{
            .size = upload_size,
            .usage = vk::BufferUsageFlagBits::eTransferSrc,
            .sharingMode = vk::SharingMode::eExclusive
        };
        std::tie(cache.staging_buffer, cache.alloc) = cache.state.allocator.createBuffer(buffer_info, vkutil::vma_mapped_alloc, cache.alloc_info);
    }

    memcpy(cache.alloc_info.pMappedData, text_data, upload_size);
    vk::CommandBuffer cmd_buffer = vkutil::create_single_time_command(cache.state.device, cache.state.transfer_command_pool);

    vk::ImageSubresourceRange range{
        .aspectMask = vk::ImageAspectFlagBits::eColor,
        .baseMipLevel = mip_index,
        .levelCount = 1,
        .baseArrayLayer = static_cast<uint32_t>(face),
        .layerCount = 1
    };
    vkutil::transition_image_layout(cmd_buffer, image.image, vkutil::ImageLayout::Undefined, vkutil::ImageLayout::TransferDst, range);
    vk::ImageSubresourceLayers layer{
        .aspectMask = vk::ImageAspectFlagBits::eColor,
        .mipLevel = mip_index,
        .baseArrayLayer = static_cast<uint32_t>(face),
        .layerCount = 1
    };
    vk::BufferImageCopy region{
        .bufferOffset = 0,
        .bufferRowLength = static_cast<uint32_t>(pixels_per_stride),
        .bufferImageHeight = height,
        .imageSubresource = layer,
        .imageOffset = { 0, 0, 0 },
        .imageExtent = { width, height, 1 }
    };
    cmd_buffer.copyBufferToImage(cache.staging_buffer, image.image, vk::ImageLayout::eTransferDstOptimal, region);
    vkutil::transition_image_layout(cmd_buffer, image.image, vkutil::ImageLayout::TransferDst, vkutil::ImageLayout::SampledImage, range);
    cmd_buffer.end();

    vk::Device device = cache.state.device;

    vk::SubmitInfo submit_info{};
    submit_info.setCommandBuffers(cmd_buffer);
    cache.state.transfer_queue.submit(submit_info, cache.wait_fence);
    auto result = device.waitForFences(cache.wait_fence, VK_TRUE, std::numeric_limits<uint64_t>::max());
    if (result != vk::Result::eSuccess) {
        LOG_ERROR("Could not wait for fences.");
        assert(false);
        return;
    }
    device.resetFences(cache.wait_fence);
    device.freeCommandBuffers(cache.state.transfer_command_pool, cmd_buffer);
}
} // namespace texture

} // namespace renderer::vulkan