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

#include <renderer/metal/functions.h>

#include "gxm_to_metal.h"
#include "mtl_types.h"

#include <gxm/functions.h>
#include <gxm/types.h>
#include <renderer/functions.h>
#include <util/align.h>
#include <util/log.h>

namespace renderer::metal {

static bool is_depth_stencil_compatible_format(SceGxmTextureBaseFormat format, bool &can_be_depth) {
    switch (format) {
        // D16 format
    case SCE_GXM_TEXTURE_BASE_FORMAT_U16:
        // D32 format
    case SCE_GXM_TEXTURE_BASE_FORMAT_F32:
        // D32M format
    case SCE_GXM_TEXTURE_BASE_FORMAT_F32M:
        // D24S8 format
    case SCE_GXM_TEXTURE_BASE_FORMAT_X8U24:
        can_be_depth = true;
        [[fallthrough]];
        // 8bit stencil
    case SCE_GXM_TEXTURE_BASE_FORMAT_U8:
    case SCE_GXM_TEXTURE_BASE_FORMAT_S8:
        return true;
    default:
        return false;
    }
}

MetalTextureCache::MetalTextureCache(MetalState &state)
    : state(state) {}

void MetalTextureCache::cleanup() {
    for (auto &entry : textures)
        entry = {};

    samplers.clear();

    for (auto &staging : staging_buffers) {
        staging.buffer = nil;
        staging.size = 0;
        staging.waiting_cmd = nil;
        staging.scene_timestamp = ~0;
        staging.frame_timestamp = ~0;
    }

    staging_idx = 0;
    last_waited_scene = 0;
    current_texture = nullptr;
    is_texture_transfer_ready = false;
}

void sync_texture(MetalContext &context, MemState &mem, std::size_t index, SceGxmTexture texture, const Config &config) {
    @autoreleasepool {
        MetalContextImpl &ctx = *context.impl;
        MetalDevice &dev = *context.state.device;

        // textures are synced right before the draw, start a new recording now if the macroblock has changed
        ctx.check_for_macroblock_change(false);

        const bool is_vertex = index >= SCE_GXM_MAX_TEXTURE_UNITS;

        const SceGxmTextureFormat format = gxm::get_format(texture);
        const SceGxmTextureBaseFormat base_format = gxm::get_base_format(format);
        if (gxm::is_paletted_format(base_format) && texture.palette_addr == 0) {
            LOG_WARN("Ignoring null palette texture");
            return;
        }

        if (is_vertex) {
            context.shader_hints.vertex_textures[index - SCE_GXM_MAX_TEXTURE_UNITS] = format;
        } else {
            context.shader_hints.fragment_textures[index] = format;
        }

        std::optional<TextureLookupResult> lookup_result = std::nullopt;

        SceGxmColorBaseFormat format_target_of_texture;
        TextureViewport texture_viewport{};

        if (renderer::texture::convert_base_texture_format_to_base_color_format(base_format, format_target_of_texture)) {
            // try to retrieve it from the color surface cache
            lookup_result = dev.surface_cache.retrieve_color_surface_as_texture(texture, format_target_of_texture, &texture_viewport);
        }

        bool is_depth_surface = false;
        if (!lookup_result.has_value() && is_depth_stencil_compatible_format(base_format, is_depth_surface)) {
            // Try to retrieve depth/stencil cache
            lookup_result = dev.surface_cache.retrieve_depth_stencil_as_texture(texture, &texture_viewport);
        }

        if (lookup_result.has_value()) {
            // get the sampler now
            dev.texture_cache.cache_and_bind_sampler(texture, is_depth_surface);
        } else {
            dev.texture_cache.cache_and_bind_texture(texture, mem);
            if (!dev.texture_cache.current_texture || !dev.texture_cache.current_texture->texture)
                return;
            auto &entry = *dev.texture_cache.current_texture;
            lookup_result = TextureLookupResult{ entry.texture, entry.format };
        }

        if (!lookup_result->texture)
            return;

        id<MTLSamplerState> sampler = dev.texture_cache.get_retrieved_sampler();

        BoundTexture &bound = is_vertex ? ctx.vertex_textures[index - SCE_GXM_MAX_TEXTURE_UNITS] : ctx.fragment_textures[index];
        if (bound.sampler != sampler || bound.texture != lookup_result->texture) {
            bound.texture = lookup_result->texture;
            bound.sampler = sampler;
            if (is_vertex)
                ctx.vertex_textures_dirty = true;
            else
                ctx.fragment_textures_dirty = true;
        }

        if (context.state.features.use_texture_viewport) {
            if (is_vertex) {
                ctx.curr_vert_ublock.set_viewport_ratio(index - SCE_GXM_MAX_TEXTURE_UNITS, texture_viewport.ratio);
                ctx.curr_vert_ublock.set_viewport_offset(index - SCE_GXM_MAX_TEXTURE_UNITS, texture_viewport.offset);
            } else {
                ctx.curr_frag_ublock.set_viewport_ratio(index, texture_viewport.ratio);
                ctx.curr_frag_ublock.set_viewport_offset(index, texture_viewport.offset);
            }
        }
    }
}

void MetalTextureCache::prepare_staging_buffer(bool is_configure) {
    assert(!is_texture_transfer_ready);
    MetalContextImpl *context = static_cast<MetalContext *>(state.context)->impl.get();
    MetalDevice &dev = *state.device;

    if (!context->is_recording) {
        LOG_ERROR("Texture upload while not recording a scene");
        return;
    }

    TextureStagingBuffer *staging_buffer = &staging_buffers[staging_idx];
    // some textures must be 16-bytes aligned
    staging_buffer->used_so_far = align(staging_buffer->used_so_far, 16);
    // we can keep using the same buffer as before if we are in the same scene and there is enough memory left
    const bool use_previous_buffer = (current_scene_timestamp == staging_buffer->scene_timestamp)
        && (staging_buffer->size - staging_buffer->used_so_far) >= current_texture->memory_needed;

    if (!use_previous_buffer) {
        staging_idx = (staging_idx + 1) % NB_TEXTURE_STAGING_BUFFERS;
        staging_buffer = &staging_buffers[staging_idx];
    }

    // if we are not using the previous buffer, we wait if the buffer was used at least once,
    // less than MAX_FRAMES_RENDERING frames ago and we have not yet waited for it
    const bool need_wait = !use_previous_buffer
        && is_frame_timestamp_in_flight(staging_buffer->frame_timestamp, dev.frame_timestamp)
        && staging_buffer->scene_timestamp > last_waited_scene;

    if (need_wait) {
        if (staging_buffer->scene_timestamp == current_scene_timestamp) {
            // special case, all the staging buffer are occupied by the current scene
            // submit the prerender command buffer and wait for it
            id<MTLCommandBuffer> previous_prerender = context->prerender_cmd;
            context->flush_prerender();
            [previous_prerender waitUntilCompleted];

            // then set all buffers as unused
            for (auto &buffer : staging_buffers) {
                buffer.frame_timestamp = ~0;
                buffer.scene_timestamp = ~0;
                buffer.waiting_cmd = nil;
            }
        } else if (staging_buffer->waiting_cmd) {
            [staging_buffer->waiting_cmd waitUntilCompleted];
            last_waited_scene = staging_buffer->scene_timestamp;
        }
    }

    if (!use_previous_buffer) {
        staging_buffer->scene_timestamp = current_scene_timestamp;
        staging_buffer->frame_timestamp = dev.frame_timestamp;
        staging_buffer->used_so_far = 0;

        if (staging_buffer->size < current_texture->memory_needed) {
            // we need to create a bigger buffer
            staging_buffer->buffer = create_shared_buffer(dev.device, current_texture->memory_needed, "texture staging");
            staging_buffer->size = current_texture->memory_needed;
        }
    }
    // the buffer is used by the current prerender command buffer
    staging_buffer->waiting_cmd = context->prerender_cmd;

    is_texture_transfer_ready = true;
}

bool MetalTextureCache::init(const bool hashless_texture_cache, const fs::path &texture_folder, const std::string_view game_id) {
    MetalDevice &dev = *state.device;

    constexpr size_t max_sampler_used = 512;
    TextureCache::init(hashless_texture_cache, texture_folder, game_id, max_sampler_used);
    backend = Backend::Metal;

    samplers.resize(max_sampler_used);

    support_depth_linear_filtering = true;
    // D24S8 does not exist on Metal, depth textures are converted to F32
    support_x8d24 = false;
    support_e5rgb9 = true;
    support_a2rgb10 = true;
    // TODO: PVRTC is natively supported by Apple GPUs but the Vita swizzle must be handled first
    support_pvrt = false;
    support_dxt = dev.support_bc;
    support_astc = dev.support_astc;

    return true;
}

void MetalTextureCache::select(size_t index, const SceGxmTexture &texture) {
    current_texture = &textures[index];
    is_texture_transfer_ready = false;
}

static MTLPixelFormat bcn_to_rgba8(const MTLPixelFormat format) {
    switch (format) {
    case MTLPixelFormatBC4_RUnorm:
        return MTLPixelFormatR8Unorm;
    case MTLPixelFormatBC4_RSnorm:
        return MTLPixelFormatR8Snorm;
    case MTLPixelFormatBC5_RGUnorm:
        return MTLPixelFormatRG8Unorm;
    case MTLPixelFormatBC5_RGSnorm:
        return MTLPixelFormatRG8Snorm;
    default:
        // BC1/2/3
        return MTLPixelFormatRGBA8Unorm;
    }
}

static bool is_compressed_format(MTLPixelFormat format) {
    return (format >= MTLPixelFormatBC1_RGBA && format <= MTLPixelFormatBC7_RGBAUnorm_sRGB)
        || (format >= MTLPixelFormatASTC_4x4_sRGB && format <= MTLPixelFormatASTC_12x12_LDR);
}

static uint32_t compressed_block_bytes(MTLPixelFormat format) {
    switch (format) {
    case MTLPixelFormatBC1_RGBA:
    case MTLPixelFormatBC1_RGBA_sRGB:
    case MTLPixelFormatBC4_RUnorm:
    case MTLPixelFormatBC4_RSnorm:
        return 8;
    default:
        return 16;
    }
}

static uint32_t get_image_memory_upper_bound(const SceGxmTexture &gxm_texture, const MTLPixelFormat format, const SceGxmTextureBaseFormat base_format) {
    // we only want an upper bound, it doesn't matter if because of some conversions we later get the width instead of the stride
    // aligning to 8 takes care of the default stride values
    uint32_t stride = std::max(next_power_of_two(gxm::get_width(gxm_texture)), 8U);
    uint32_t height = std::max(next_power_of_two(gxm::get_height(gxm_texture)), 8U);

    if (gxm_texture.texture_type() == SCE_GXM_TEXTURE_LINEAR_STRIDED) {
        const uint32_t bpp = (gxm::bits_per_pixel(base_format) + 7) / 8;
        // the max is to handle the case of P4 textures
        stride = std::max<uint32_t>(stride, gxm::get_stride_in_bytes(gxm_texture) / bpp);
    }

    if (is_compressed_format(format)) {
        // compressed formats use at most 16 bytes per 4x4 block, astc blocks can be bigger than 4x4
        return align(stride, 4) * align(height, 4);
    }

    return stride * height * bytes_per_pixel(format);
}

void MetalTextureCache::create_current_texture(uint32_t width, uint32_t height, MTLPixelFormat format, bool is_cube, uint16_t mip_count, const MTLTextureSwizzleChannels &swizzle) {
    MetalDevice &dev = *state.device;

    MTLTextureDescriptor *desc = [[MTLTextureDescriptor alloc] init];
    desc.textureType = is_cube ? MTLTextureTypeCube : MTLTextureType2D;
    desc.pixelFormat = format;
    desc.width = width;
    desc.height = height;
    desc.depth = 1;
    desc.mipmapLevelCount = mip_count;
    desc.arrayLength = 1;
    desc.sampleCount = 1;
    desc.usage = MTLTextureUsageShaderRead;
    desc.storageMode = MTLStorageModePrivate;
    desc.swizzle = swizzle;

    // In case the cache is full, the previous texture is kept alive by Metal while it is in use
    current_texture->texture = [dev.device newTextureWithDescriptor:desc];
    if (!current_texture->texture)
        LOG_ERROR("Failed to create a {}x{} texture with format {}", width, height, format_name(format));
    current_texture->format = format;
    current_texture->width = width;
    current_texture->height = height;
    current_texture->is_cube = is_cube;
    current_texture->mip_count = mip_count;
}

void MetalTextureCache::configure_texture(const SceGxmTexture &gxm_texture) {
    @autoreleasepool {
        MetalDevice &dev = *state.device;
        const SceGxmTextureFormat format = gxm::get_format(gxm_texture);
        const SceGxmTextureBaseFormat base_format = gxm::get_base_format(format);

        const MTLTextureSwizzleChannels swizzle = texture::translate_swizzle(format);

        const bool is_cube = (gxm_texture.texture_type() == SCE_GXM_TEXTURE_CUBE || gxm_texture.texture_type() == SCE_GXM_TEXTURE_CUBE_ARBITRARY);

        const uint32_t width = gxm::get_width(gxm_texture);
        const uint32_t height = gxm::get_height(gxm_texture);

        const uint16_t mip_count = renderer::texture::get_upload_mip(gxm_texture.true_mip_count(), width, height);

        MTLPixelFormat mtl_format = texture::translate_format(base_format, dev.support_packed16);
        if (gxm::is_bcn_format(base_format) && !support_dxt)
            // texture will be decompressed
            mtl_format = bcn_to_rgba8(mtl_format);
        if (gxm_texture.gamma_mode)
            mtl_format = linear_to_srgb(mtl_format);

        uint32_t memory_needed = get_image_memory_upper_bound(gxm_texture, mtl_format, base_format);
        if (mip_count > 1)
            // using mips, the overall memory needed will be 4/3 of the base memory
            // round up to 3/2
            memory_needed += memory_needed / 2;
        if (is_cube)
            memory_needed *= 6;
        current_texture->memory_needed = align(memory_needed, 16);

        create_current_texture(width, height, mtl_format, is_cube, mip_count, swizzle);

        if (!gxm_texture.normalize_mode)
            LOG_ERROR("Unhandled unnormalized texture, please report it to the developers");

        prepare_staging_buffer(true);
    }
}

static void *add_alpha_channel(const void *pixels, const uint32_t width, const uint32_t height, std::vector<uint8_t> &data) {
    data.resize(width * height * 4);

    const uint8_t *src = static_cast<const uint8_t *>(pixels);
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

// convert 16-bit packed formats (with the layouts used by the Vulkan renderer) to rgba8 when they are not supported
static void *unpack_16bit_format(SceGxmTextureBaseFormat base_format, const void *pixels, const uint32_t width, const uint32_t height, std::vector<uint8_t> &data) {
    data.resize(width * height * 4);
    const uint16_t *src = static_cast<const uint16_t *>(pixels);
    uint8_t *dst = data.data();
    for (uint32_t i = 0; i < width * height; i++) {
        const uint16_t value = src[i];
        switch (base_format) {
        case SCE_GXM_TEXTURE_BASE_FORMAT_U4U4U4U4:
            // R4G4B4A4 pack16: R in the 4 most significant bits
            dst[0] = static_cast<uint8_t>(((value >> 12) & 0xF) * 17);
            dst[1] = static_cast<uint8_t>(((value >> 8) & 0xF) * 17);
            dst[2] = static_cast<uint8_t>(((value >> 4) & 0xF) * 17);
            dst[3] = static_cast<uint8_t>((value & 0xF) * 17);
            break;
        case SCE_GXM_TEXTURE_BASE_FORMAT_U5U6U5:
            // R5G6B5 pack16
            dst[0] = static_cast<uint8_t>(((value >> 11) & 0x1F) * 255 / 31);
            dst[1] = static_cast<uint8_t>(((value >> 5) & 0x3F) * 255 / 63);
            dst[2] = static_cast<uint8_t>((value & 0x1F) * 255 / 31);
            dst[3] = 255;
            break;
        default:
            // A1R5G5B5 pack16
            dst[0] = static_cast<uint8_t>(((value >> 10) & 0x1F) * 255 / 31);
            dst[1] = static_cast<uint8_t>(((value >> 5) & 0x1F) * 255 / 31);
            dst[2] = static_cast<uint8_t>((value & 0x1F) * 255 / 31);
            dst[3] = (value & 0x8000) ? 255 : 0;
            break;
        }
        dst += 4;
    }
    return data.data();
}

void MetalTextureCache::upload_texture_impl(SceGxmTextureBaseFormat base_format, uint32_t width, uint32_t height,
    uint32_t mip_index, const void *pixels, int face, uint32_t pixels_per_stride) {
    @autoreleasepool {
        if (!is_texture_transfer_ready)
            prepare_staging_buffer();

        if (!is_texture_transfer_ready || !current_texture->texture)
            return;

        MetalContextImpl *context = static_cast<MetalContext *>(state.context)->impl.get();
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
        } else if (!state.device->support_packed16
            && (base_format == SCE_GXM_TEXTURE_BASE_FORMAT_U4U4U4U4 || base_format == SCE_GXM_TEXTURE_BASE_FORMAT_U5U6U5 || base_format == SCE_GXM_TEXTURE_BASE_FORMAT_U1U5U5U5)) {
            text_data = unpack_16bit_format(base_format, pixels, pixels_per_stride, height, temp_data);
            base_format = SCE_GXM_TEXTURE_BASE_FORMAT_U8U8U8U8;
        }

        const MTLPixelFormat format = current_texture->format;
        uint32_t upload_size;
        uint32_t bytes_per_row;
        uint32_t rows;
        if (gxm::is_bcn_format(base_format)) {
            upload_size = renderer::texture::get_compressed_size(base_format, pixels_per_stride, height);
            bytes_per_row = (align(pixels_per_stride, 4) / 4) * compressed_block_bytes(format);
            rows = align(height, 4) / 4;
        } else if (renderer::texture::is_astc_format(base_format)) {
            upload_size = renderer::texture::get_compressed_size(base_format, pixels_per_stride, height);
            const auto [block_width, block_height] = gxm::get_block_size(base_format);
            bytes_per_row = ((pixels_per_stride + block_width - 1) / block_width) * 16;
            rows = (height + block_height - 1) / block_height;
        } else {
            const size_t bpp = gxm::bits_per_pixel(base_format);
            const size_t bytes_per_pixel = (bpp + 7) >> 3;
            upload_size = static_cast<uint32_t>(pixels_per_stride * height * bytes_per_pixel);
            bytes_per_row = static_cast<uint32_t>(pixels_per_stride * bytes_per_pixel);
            rows = height;
        }

        // the source offset must be aligned
        staging_buffer.used_so_far = align(staging_buffer.used_so_far, 16);
        if (staging_buffer.used_so_far + upload_size > staging_buffer.size) {
            LOG_ERROR("Staging buffer size left ({}) is too small for texture size {}!", staging_buffer.size - staging_buffer.used_so_far, upload_size);
            return;
        }

        memcpy(static_cast<uint8_t *>(staging_buffer.buffer.contents) + staging_buffer.used_so_far, text_data, upload_size);

        id<MTLBlitCommandEncoder> blit = context->blit();
        [blit copyFromBuffer:staging_buffer.buffer
                   sourceOffset:staging_buffer.used_so_far
              sourceBytesPerRow:bytes_per_row
            sourceBytesPerImage:static_cast<NSUInteger>(bytes_per_row) * rows
                     sourceSize:MTLSizeMake(width, height, 1)
                      toTexture:current_texture->texture
               destinationSlice:static_cast<NSUInteger>(face)
               destinationLevel:mip_index
              destinationOrigin:MTLOriginMake(0, 0, 0)];

        staging_buffer.used_so_far += upload_size;
    }
}

void MetalTextureCache::upload_done() {
    is_texture_transfer_ready = false;
}

void MetalTextureCache::configure_sampler(size_t index, const SceGxmTexture &texture, bool no_linear) {
    @autoreleasepool {
        // linear strided textures use the mag filter as the min filter too
        const bool is_linear_strided = texture.texture_type() == SCE_GXM_TEXTURE_LINEAR_STRIDED;

        const SceGxmTextureAddrMode uaddr = static_cast<SceGxmTextureAddrMode>(texture.uaddr_mode);
        const SceGxmTextureAddrMode vaddr = static_cast<SceGxmTextureAddrMode>(texture.vaddr_mode);
        SceGxmTextureFilter mag_filter = static_cast<SceGxmTextureFilter>(texture.mag_filter);
        SceGxmTextureFilter min_filter = is_linear_strided ? mag_filter : static_cast<SceGxmTextureFilter>(texture.min_filter);

        if (no_linear) {
            min_filter = SCE_GXM_TEXTURE_FILTER_POINT;
            mag_filter = SCE_GXM_TEXTURE_FILTER_POINT;
        }

        MTLSamplerDescriptor *desc = [[MTLSamplerDescriptor alloc] init];
        desc.magFilter = texture::translate_filter(mag_filter);
        desc.minFilter = texture::translate_filter(min_filter);
        desc.mipFilter = texture.mip_filter ? MTLSamplerMipFilterLinear : MTLSamplerMipFilterNearest;
        desc.sAddressMode = texture::translate_address_mode(uaddr);
        desc.tAddressMode = texture::translate_address_mode(vaddr);
        desc.rAddressMode = MTLSamplerAddressModeRepeat;
        desc.lodMinClamp = static_cast<float>(texture.true_lod_min());
        desc.lodMaxClamp = FLT_MAX;
        desc.normalizedCoordinates = YES;
        desc.borderColor = MTLSamplerBorderColorOpaqueBlack;

        // when using nearest filter, disable anisotropy as the pixels can contain data other than color
        const bool use_anisotropy = (anisotropic_filtering > 1) && (desc.magFilter != MTLSamplerMinMagFilterNearest || desc.minFilter != MTLSamplerMinMagFilterNearest);
        desc.maxAnisotropy = use_anisotropy ? std::clamp(anisotropic_filtering, 1, 16) : 1;

        samplers[index] = [state.device->device newSamplerStateWithDescriptor:desc];
    }
}

void MetalTextureCache::import_configure_impl(SceGxmTextureBaseFormat base_format, uint32_t width, uint32_t height, bool is_srgb, uint16_t nb_components, uint16_t mipcount, bool swap_rb) {
    @autoreleasepool {
        MetalDevice &dev = *state.device;
        uint32_t texture_size;
        if (renderer::texture::is_astc_format(base_format)) {
            texture_size = renderer::texture::get_compressed_size(base_format, width, height);
        } else {
            const size_t bpp = gxm::bits_per_pixel(base_format);
            texture_size = static_cast<uint32_t>((align(width, 4) * align(height, 4) * bpp) / 8);
        }
        current_texture->memory_needed = align(texture_size, 16);

        if (mipcount > 1)
            current_texture->memory_needed += current_texture->memory_needed / 2;

        const bool is_cube = current_info->texture.texture_type() == SCE_GXM_TEXTURE_CUBE || current_info->texture.texture_type() == SCE_GXM_TEXTURE_CUBE_ARBITRARY;
        if (is_cube)
            current_texture->memory_needed *= 6;

        MTLPixelFormat format = texture::translate_format(base_format, dev.support_packed16);
        if (is_srgb)
            format = linear_to_srgb(format);

        MTLTextureSwizzleChannels swizzle = identity_swizzle;
        if (nb_components == 3) {
            swizzle.alpha = MTLTextureSwizzleOne;
        } else if (nb_components <= 2) {
            // use the real swizzle
            swizzle = texture::translate_swizzle(gxm::get_format(current_info->texture));
        }

        // u5u6u5 is stored as bgr
        if (base_format == SCE_GXM_TEXTURE_BASE_FORMAT_U5U6U5) {
            swap_rb = !swap_rb;
        }

        // this format is stored as abgr
        if (base_format == SCE_GXM_TEXTURE_BASE_FORMAT_U4U4U4U4) {
            std::swap(swizzle.red, swizzle.alpha);
            std::swap(swizzle.green, swizzle.blue);
        }

        if (swap_rb)
            std::swap(swizzle.red, swizzle.blue);

        create_current_texture(width, height, format, is_cube, mipcount, swizzle);

        prepare_staging_buffer(true);
    }
}

} // namespace renderer::metal
