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

#include <renderer/functions.h>

#include <renderer/profile.h>
#include <renderer/texture_cache.h>

#include <gxm/functions.h>
#include <mem/ptr.h>
#include <util/align.h>
#include <util/log.h>

#include <algorithm>
#include <cstring>
#include <numeric>
#include <xxh3.h>
#ifdef WIN32
#include <execution>
#endif

namespace renderer {
namespace texture {

static uint64_t hash_data(const void *data, size_t size) {
    return XXH_INLINE_XXH3_64bits(data, size);
}

static uint64_t hash_palette_data(const SceGxmTexture &texture, size_t count, const MemState &mem) {
    const uint32_t *const palette_bytes = get_texture_palette(texture, mem);
    return hash_data(palette_bytes, count * sizeof(uint32_t));
}

uint64_t hash_texture_data(const SceGxmTexture &texture, const MemState &mem) {
    R_PROFILE(__func__);
    const SceGxmTextureFormat format = gxm::get_format(&texture);
    const SceGxmTextureBaseFormat base_format = gxm::get_base_format(format);
    const size_t size = texture_size(texture);
    const Ptr<const void> data(texture.data_addr << 2);
    uint64_t data_hash = 0;

    if (data.address()) {
        data_hash = hash_data(data.get(mem), size);
    }

    switch (base_format) {
    case SCE_GXM_TEXTURE_BASE_FORMAT_P4:
        return data_hash ^ hash_palette_data(texture, 16, mem);
    case SCE_GXM_TEXTURE_BASE_FORMAT_P8:
        return data_hash ^ hash_palette_data(texture, 256, mem);
    default:
        return data_hash;
    }
}

bool can_texture_be_unswizzled_without_decode(SceGxmTextureBaseFormat fmt, bool is_vulkan) {
    return fmt == SCE_GXM_TEXTURE_BASE_FORMAT_P4
        || fmt == SCE_GXM_TEXTURE_BASE_FORMAT_U4U4U4U4
        || fmt == SCE_GXM_TEXTURE_BASE_FORMAT_P8
        || fmt == SCE_GXM_TEXTURE_BASE_FORMAT_U8
        || fmt == SCE_GXM_TEXTURE_BASE_FORMAT_U5U6U5
        || fmt == SCE_GXM_TEXTURE_BASE_FORMAT_U1U5U5U5
        || fmt == SCE_GXM_TEXTURE_BASE_FORMAT_U8U3U3U2
        || fmt == SCE_GXM_TEXTURE_BASE_FORMAT_U8U8
        || fmt == SCE_GXM_TEXTURE_BASE_FORMAT_U8U8U8
        || fmt == SCE_GXM_TEXTURE_BASE_FORMAT_S8S8S8
        || fmt == SCE_GXM_TEXTURE_BASE_FORMAT_U8U8U8U8
        || fmt == SCE_GXM_TEXTURE_BASE_FORMAT_F16F16F16F16
        || (is_vulkan && fmt == SCE_GXM_TEXTURE_BASE_FORMAT_SE5M9M9M9);
}

static bool is_block_compressed_format(SceGxmTextureBaseFormat fmt) {
    return (fmt == SCE_GXM_TEXTURE_BASE_FORMAT_UBC1
        || fmt == SCE_GXM_TEXTURE_BASE_FORMAT_UBC2
        || fmt == SCE_GXM_TEXTURE_BASE_FORMAT_UBC3
        || fmt == SCE_GXM_TEXTURE_BASE_FORMAT_UBC4
        || fmt == SCE_GXM_TEXTURE_BASE_FORMAT_UBC5
        || fmt == SCE_GXM_TEXTURE_BASE_FORMAT_PVRT2BPP
        || fmt == SCE_GXM_TEXTURE_BASE_FORMAT_PVRTII2BPP
        || fmt == SCE_GXM_TEXTURE_BASE_FORMAT_PVRT4BPP
        || fmt == SCE_GXM_TEXTURE_BASE_FORMAT_PVRTII4BPP);
}

uint16_t get_upload_mip(const uint16_t true_mip, const uint16_t width, const uint16_t height, const SceGxmTextureBaseFormat base_format) {
    uint16_t max_mip_text;

    if (is_block_compressed_format(base_format)) {
        // blocks size is 4x4, do not try to upload mips whose with or height is not a multiple of 4
        max_mip_text = std::bit_width(std::min<uint16_t>(width & (-width), height & (-height)));
        max_mip_text -= 2;
    } else {
        max_mip_text = std::bit_width(std::min(width, height));
    }

    return std::min(true_mip, max_mip_text);
}
} // namespace texture

using namespace texture;

bool TextureCache::init(const bool hashless_texture_cache) {
    use_protect = hashless_texture_cache;

    // initialize the doubly linked list
    for (int i = 0; i < TextureCacheSize; i++) {
        infoes[i].prev = &infoes[i - 1];
        infoes[i].next = &infoes[i + 1];
    }
    // fix the first and last elements
    infoes[0].prev = &infoes[TextureCacheSize - 1];
    infoes[TextureCacheSize - 1].next = &infoes[0];

    // set the list head to any element;
    info_list_head = &infoes[0];

    // prevent stutter caused by the hashmap resizing
    info_lookup.reserve(TextureCacheSize);

    return true;
}

void TextureCache::upload_texture(const SceGxmTexture &gxm_texture, MemState &mem) {
    R_PROFILE(__func__);

    bool is_vulkan = (backend == renderer::Backend::Vulkan);

    const SceGxmTextureFormat fmt = gxm::get_format(&gxm_texture);
    const SceGxmTextureBaseFormat base_format = gxm::get_base_format(fmt);

    const bool block_compressed = renderer::texture::is_compressed_format(base_format);
    auto width = static_cast<uint32_t>(gxm::get_width(&gxm_texture));
    auto height = static_cast<uint32_t>(gxm::get_height(&gxm_texture));
    if (block_compressed) {
        // align width and height to block size
        width = (width + 3) & ~3;
        height = (height + 3) & ~3;
    }

    const Ptr<uint8_t> data(gxm_texture.data_addr << 2);
    uint8_t *texture_data = data.get(mem);

    if (!texture_data) {
        return;
    }

    std::vector<uint8_t> texture_data_decompressed;
    std::vector<uint8_t> texture_pixels_lineared; // TODO Move to context to avoid frequent allocation?
    std::vector<uint32_t> palette_texture_pixels;
    std::vector<uint8_t> yuv_texture_pixels;

    const void *pixels = nullptr;

    size_t pixels_per_stride = 0;
    size_t bpp = bits_per_pixel(base_format);
    size_t bytes_per_pixel = (bpp + 7) >> 3;

    const auto texture_type = gxm_texture.texture_type();
    const bool is_swizzled = (texture_type == SCE_GXM_TEXTURE_SWIZZLED) || (texture_type == SCE_GXM_TEXTURE_CUBE) || (texture_type == SCE_GXM_TEXTURE_SWIZZLED_ARBITRARY) || (texture_type == SCE_GXM_TEXTURE_CUBE_ARBITRARY);
    const bool need_unswizzle = is_swizzled && block_compressed;
    const bool need_decompress_and_unswizzle_on_cpu = is_swizzled && !block_compressed && !texture::can_texture_be_unswizzled_without_decode(base_format, is_vulkan);

    uint32_t mip_index = 0;
    uint32_t total_mip = get_upload_mip(gxm_texture.true_mip_count(), width, height, base_format);
    uint32_t face_uploaded_count = 0;
    uint32_t face_total_count;
    size_t source_size = 0;
    size_t total_source_so_far = 0;

    // Modified during decompression
    uint32_t org_width = width;
    uint32_t org_height = height;

    uint32_t org_width_const = width;
    uint32_t org_height_const = height;

    uint32_t face_align_bytes = 4;

    if (texture_type == SCE_GXM_TEXTURE_LINEAR_STRIDED) {
        total_mip = 1;
    }

    // > 0 means texture cube
    int upload_type = 0;

    face_total_count = 1;
    if ((texture_type == SCE_GXM_TEXTURE_CUBE) || (texture_type == SCE_GXM_TEXTURE_CUBE_ARBITRARY)) {
        upload_type = 1;
        face_total_count = 6;

        const bool twok_align_cond1 = ((width >= 32) && (height >= 32) && ((bytes_per_pixel == 1) || (texture::is_block_compressed_format(base_format))));
        const bool twok_align_cond2 = ((width >= 16) && (height >= 16) && ((bytes_per_pixel == 2) || (bytes_per_pixel == 4)));
        const bool twok_align_cond3 = ((width >= 8) && (height >= 8) && (bytes_per_pixel == 8));

        if (twok_align_cond1 || twok_align_cond2 || twok_align_cond3) {
            face_align_bytes = 2048;
        }
    }

    while ((face_uploaded_count < face_total_count) && org_width && org_height) {
        width = org_width;
        height = org_height;
        pixels = texture_data;

        SceGxmTextureBaseFormat upload_format = base_format;

        // Get pixels per stride
        switch (texture_type) {
        case SCE_GXM_TEXTURE_SWIZZLED_ARBITRARY:
        case SCE_GXM_TEXTURE_CUBE_ARBITRARY:
            width = next_power_of_two(width);
            height = next_power_of_two(height);
        case SCE_GXM_TEXTURE_SWIZZLED:
        case SCE_GXM_TEXTURE_CUBE:
        case SCE_GXM_TEXTURE_TILED:
            pixels_per_stride = static_cast<size_t>(width);
            break;
        case SCE_GXM_TEXTURE_LINEAR:
            pixels_per_stride = static_cast<size_t>((width + 7) & ~7);
            break;
        case SCE_GXM_TEXTURE_LINEAR_STRIDED:
            pixels_per_stride = gxm::get_stride_in_bytes(&gxm_texture) / bytes_per_pixel;
            if (base_format == SCE_GXM_TEXTURE_BASE_FORMAT_P4) // P4 textures are the only one not byte aligned, therefore bytes_per_pixel should be 0.5 and not 1, correct it here
                pixels_per_stride *= 2;
            break;
        }

        if (gxm::is_paletted_format(base_format)) {
            palette_texture_pixels.resize(width * height * 4);
            if (base_format == SCE_GXM_TEXTURE_BASE_FORMAT_P8) {
                renderer::texture::palette_texture_to_rgba_8(palette_texture_pixels.data(),
                    reinterpret_cast<const uint8_t *>(pixels), width, height, pixels_per_stride, renderer::texture::get_texture_palette(gxm_texture, mem));
            } else {
                renderer::texture::palette_texture_to_rgba_4(reinterpret_cast<uint32_t *>(palette_texture_pixels.data()),
                    reinterpret_cast<const uint8_t *>(pixels), width, height, pixels_per_stride / 2, renderer::texture::get_texture_palette(gxm_texture, mem));
            }
            pixels = palette_texture_pixels.data();
            bytes_per_pixel = 4;
            bpp = 32;
            upload_format = SCE_GXM_TEXTURE_BASE_FORMAT_U8U8U8U8;
        }

        if (need_unswizzle) {
            // Must unswizzle them
            texture_data_decompressed.resize(renderer::texture::get_compressed_size(base_format, width, height));
            resolve_z_order_compressed_texture(base_format, texture_data_decompressed.data(), pixels, width, height);
            pixels = texture_data_decompressed.data();
        } else if (need_decompress_and_unswizzle_on_cpu) {
            // Must decompress them
            texture_data_decompressed.resize(align(width, 4) * align(height, 4) * 4);
            source_size = decompress_compressed_swizz_texture(base_format, texture_data_decompressed.data(), pixels, width, height);
            bytes_per_pixel = 4;
            bpp = 32;
            upload_format = SCE_GXM_TEXTURE_BASE_FORMAT_U8U8U8U8;
            pixels = texture_data_decompressed.data();
        }

        switch (base_format) {
        case SCE_GXM_TEXTURE_BASE_FORMAT_PVRT2BPP:
        case SCE_GXM_TEXTURE_BASE_FORMAT_PVRT4BPP:
        case SCE_GXM_TEXTURE_BASE_FORMAT_PVRTII2BPP:
        case SCE_GXM_TEXTURE_BASE_FORMAT_PVRTII4BPP:
            break;
        case SCE_GXM_TEXTURE_BASE_FORMAT_U8U3U3U2:
            // Convert U8U3U3U2 to U8U8U8U8
            texture_data_decompressed.resize(width * height * 4);
            convert_U8U3U3U2_to_U8U8U8U8(texture_data_decompressed.data(), pixels, width, height, pixels_per_stride);
            pixels = texture_data_decompressed.data();
            upload_format = SCE_GXM_TEXTURE_BASE_FORMAT_U8U8U8U8;
            bpp = 32;
            break;
        case SCE_GXM_TEXTURE_BASE_FORMAT_SE5M9M9M9:
            // this format is supported on all GPUs with vulkan
            if (is_vulkan)
                break;
            texture_data_decompressed.resize(width * height * 6);
            decompress_packed_float_e5m9m9m9(base_format, texture_data_decompressed.data(), pixels, width, height);
            pixels = texture_data_decompressed.data();
            break;
        case SCE_GXM_TEXTURE_BASE_FORMAT_U2F10F10F10:
            // don't change what openGL is doing (which is completely wrong)
            if (!is_vulkan)
                break;
            texture_data_decompressed.resize(width * height * 8);
            convert_u2f10f10f10_to_f16f16f16f16(texture_data_decompressed.data(), pixels, width, height, pixels_per_stride, fmt);
            pixels = texture_data_decompressed.data();
            upload_format = SCE_GXM_TEXTURE_BASE_FORMAT_F16F16F16F16;
            break;
        case SCE_GXM_TEXTURE_BASE_FORMAT_X8U24:
            texture_data_decompressed.resize(width * height * 4);
            if (is_vulkan) {
                // d24_u8 or x8_d24 is not supported on all GPUs (thanks AMD)
                convert_x8u24_to_f32(texture_data_decompressed.data(), pixels, width, height, pixels_per_stride, fmt);
                upload_format = SCE_GXM_TEXTURE_BASE_FORMAT_F32;
            } else {
                // X8 = [24-31], D24 = [0-23], technically this is GL_UNSIGNED_INT_24_8_REV which does not exist
                // TODO: Requires shader to convert the normalized value read by GL to unsigned int. Just multiply by 2^24-1 when reading and you're done.
                // TODO: this is wrong, the depth is in the upper or lower 24 bits according to the swizzle
                convert_x8u24_to_u24x8(texture_data_decompressed.data(), pixels, width, height, pixels_per_stride);
            }
            pixels = texture_data_decompressed.data();
            pixels_per_stride = width;
            break;
        case SCE_GXM_TEXTURE_BASE_FORMAT_F32M:
            // Convert F32M to F32
            texture_data_decompressed.resize(width * height * 4);
            convert_f32m_to_f32(texture_data_decompressed.data(), pixels, width, height, pixels_per_stride);
            pixels = texture_data_decompressed.data();
            pixels_per_stride = width;
            upload_format = SCE_GXM_TEXTURE_BASE_FORMAT_F32;
            break;
        case SCE_GXM_TEXTURE_BASE_FORMAT_UBC1:
        case SCE_GXM_TEXTURE_BASE_FORMAT_UBC2:
        case SCE_GXM_TEXTURE_BASE_FORMAT_UBC3:
        case SCE_GXM_TEXTURE_BASE_FORMAT_UBC4:
        case SCE_GXM_TEXTURE_BASE_FORMAT_SBC4:
        case SCE_GXM_TEXTURE_BASE_FORMAT_UBC5:
        case SCE_GXM_TEXTURE_BASE_FORMAT_SBC5:
            source_size = renderer::texture::get_compressed_size(base_format, width, height);

            if ((texture_type == SCE_GXM_TEXTURE_SWIZZLED_ARBITRARY) || (texture_type == SCE_GXM_TEXTURE_CUBE_ARBITRARY)) {
                size_t compressed_size = renderer::texture::get_compressed_size(base_format, org_width, org_height);

                texture_pixels_lineared.resize(compressed_size);
                remove_compressed_arbitrary_blocks(base_format, texture_pixels_lineared.data(), pixels, org_width, org_height);

                pixels = texture_pixels_lineared.data();
                pixels_per_stride = org_width;

                if (need_unswizzle)
                    texture_data_decompressed.clear();
            }
            break;

        default:
            if (texture_type == SCE_GXM_TEXTURE_LINEAR || texture_type == SCE_GXM_TEXTURE_LINEAR_STRIDED)
                break;
            // Convert data to linear layout
            texture_pixels_lineared.resize(width * height * bytes_per_pixel);

            if (is_swizzled)
                renderer::texture::swizzled_texture_to_linear_texture(texture_pixels_lineared.data(), reinterpret_cast<const uint8_t *>(pixels), width, height,
                    static_cast<std::uint8_t>(bpp));
            else
                renderer::texture::tiled_texture_to_linear_texture(texture_pixels_lineared.data(), reinterpret_cast<const uint8_t *>(pixels), width, height,
                    static_cast<std::uint8_t>(bpp));

            pixels = texture_pixels_lineared.data();

            if (need_decompress_and_unswizzle_on_cpu)
                texture_data_decompressed.clear();
        }

        if ((texture_type == SCE_GXM_TEXTURE_SWIZZLED_ARBITRARY) || (texture_type == SCE_GXM_TEXTURE_CUBE_ARBITRARY)) {
            width = org_width;
            height = org_height;
        }

        if (gxm::is_paletted_format(base_format)) {
            pixels_per_stride = width;
        }

        if (gxm::is_yuv_format(base_format)) {
            switch (fmt) {
            case SCE_GXM_TEXTURE_FORMAT_YUV420P2_CSC0:
            case SCE_GXM_TEXTURE_FORMAT_YVU420P2_CSC0:
            case SCE_GXM_TEXTURE_FORMAT_YUV420P2_CSC1:
            case SCE_GXM_TEXTURE_FORMAT_YVU420P2_CSC1:
            case SCE_GXM_TEXTURE_FORMAT_YUV420P3_CSC0:
            case SCE_GXM_TEXTURE_FORMAT_YVU420P3_CSC0:
            case SCE_GXM_TEXTURE_FORMAT_YUV420P3_CSC1:
            case SCE_GXM_TEXTURE_FORMAT_YVU420P3_CSC1: {
                yuv_texture_pixels.resize(width * height * 4);
                renderer::texture::yuv420_texture_to_rgb(yuv_texture_pixels.data(),
                    reinterpret_cast<const uint8_t *>(pixels), width, height);
                pixels = yuv_texture_pixels.data();
                pixels_per_stride = width;
                bpp = 32;
                upload_format = SCE_GXM_TEXTURE_BASE_FORMAT_U8U8U8U8;
                break;
            }

            case SCE_GXM_TEXTURE_FORMAT_YUYV422_CSC0:
            case SCE_GXM_TEXTURE_FORMAT_YVYU422_CSC0:
            case SCE_GXM_TEXTURE_FORMAT_UYVY422_CSC0:
            case SCE_GXM_TEXTURE_FORMAT_VYUY422_CSC0:
            case SCE_GXM_TEXTURE_FORMAT_YUYV422_CSC1:
            case SCE_GXM_TEXTURE_FORMAT_YVYU422_CSC1:
            case SCE_GXM_TEXTURE_FORMAT_UYVY422_CSC1:
            case SCE_GXM_TEXTURE_FORMAT_VYUY422_CSC1:
                LOG_ERROR("Yuv Texture format not implemented: {}", fmt);
                assert(false);
            default:
                assert(false);
            }
        }

        if (!block_compressed && !need_decompress_and_unswizzle_on_cpu) {
            source_size = (pixels_per_stride * height * ((bpp + 7) >> 3));
        }

        upload_texture_impl(upload_format, width, height, mip_index, pixels, upload_type, block_compressed, pixels_per_stride);

        mip_index++;
        org_width /= 2;
        org_height /= 2;

        texture_data += source_size;
        total_source_so_far += source_size;

        if (mip_index == total_mip) {
            mip_index = 0;
            face_uploaded_count++;

            org_width = org_width_const;
            org_height = org_height_const;

            upload_type++;

            size_t source_unaligned_size = total_source_so_far;
            total_source_so_far = align(total_source_so_far, face_align_bytes);

            texture_data += total_source_so_far - source_unaligned_size;
        }
    }
}

void TextureCache::cache_and_bind_texture(const SceGxmTexture &gxm_texture, MemState &mem) {
    R_PROFILE(__func__);

    size_t index = 0;
    bool configure = false;
    bool upload = false;
    const size_t size = texture_size(gxm_texture);

    // Try to find GXM texture in cache.
    int cached_gxm_texture_index = -1;
    auto *gxm_texture_content = reinterpret_cast<const TextureGxmDataRepr *>(&gxm_texture);
    auto gxm_it = info_lookup.find(*gxm_texture_content);
    if (gxm_it != info_lookup.end())
        // we found the texture in the cache
        cached_gxm_texture_index = static_cast<int>(gxm_it->second - infoes.data());

    Address range_protect_begin = 0;
    Address range_protect_end = 0;
    bool should_use_hash = true;

    // To prevent protecting too commonly accessed data that belongs to the page where the texture also resides
    // (for example, uniform buffer value and texture data got mixed, so page faults are triggered too many, it's not always good).
    // This works under the assumption that once this big enough texture decided to modify. It will have to modify either all of its data,
    // or replace with an entire new texture.
    if (use_protect && size >= mem.page_size * 4) {
        range_protect_begin = align(gxm_texture.data_addr << 2, mem.page_size);
        range_protect_end = align_down((gxm_texture.data_addr << 2) + size, mem.page_size);

        if (range_protect_end - range_protect_begin >= mem.page_size * 4) {
            should_use_hash = false;
        }
    }

    TextureCacheInfo *info;
    if (cached_gxm_texture_index == -1) {
        // Texture not found in cache.
        // get the least recently used texture, which info_list_head points to
        index = static_cast<int>(info_list_head - infoes.data());
        info = &infoes[index];
        if (info->timestamp > 0) {
            // Cache is full.
            LOG_DEBUG("Evicting texture {} (t = {}) from cache. Current t = {}.", index, info->timestamp, timestamp);
            auto *previous_gxm_texture = reinterpret_cast<const TextureGxmDataRepr *>(&info->texture);
            info_lookup.erase(*previous_gxm_texture);
        }
        info_lookup[*gxm_texture_content] = info;

        configure = true;
        upload = true;
        info->texture = gxm_texture;
        info->use_hash = should_use_hash;
        if (info->use_hash) {
            info->hash = hash_texture_data(gxm_texture, mem);
        }
    } else {
        // Texture is cached.
        index = cached_gxm_texture_index;
        info = &infoes[index];
        configure = false;
        if (info->use_hash) {
            const uint64_t hash = hash_texture_data(gxm_texture, mem);
            upload = info->hash != hash;
            info->hash = hash;
        } else {
            upload = info->dirty;
        }
    }

    if (gxm_texture.data_addr == 0) {
        upload = false;
    }

    select(index, gxm_texture);

    if (configure) {
        configure_texture(gxm_texture);
    }
    if (upload) {
        upload_texture(gxm_texture, mem);
        if (!info->use_hash) {
            info->dirty = false;
            add_protect(mem, range_protect_begin, range_protect_end - range_protect_begin, MemPerm::ReadOnly, [info, gxm_texture](Address, bool) {
                if (memcmp(&info->texture, &gxm_texture, sizeof(SceGxmTexture)) == 0) {
                    info->dirty = true;
                }

                return true;
            });
        }
        upload_done();
    }

    info->timestamp = timestamp++;
    // remove the texture from its current position and insert it at the end of the circular list
    // so the circular list keeps its lru order

    // first removal (update info_list_head if necessary)
    if (info_list_head == info)
        info_list_head = info->next;
    info->next->prev = info->prev;
    info->prev->next = info->next;

    // then add it at the end
    info_list_head->prev->next = info;
    info->prev = info_list_head->prev;
    info_list_head->prev = info;
    info->next = info_list_head;
}

} // namespace renderer
