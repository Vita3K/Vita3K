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
#include <util/bit_cast.h>
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

uint64_t hash_texture_data(const SceGxmTexture &texture, uint32_t texture_size, const MemState &mem) {
    const SceGxmTextureFormat format = gxm::get_format(texture);
    const SceGxmTextureBaseFormat base_format = gxm::get_base_format(format);
    const Ptr<const void> data(texture.data_addr << 2);
    uint64_t data_hash = 0;

    if (data.address()) {
        data_hash = hash_data(data.get(mem), texture_size);
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

uint16_t get_upload_mip(const uint16_t true_mip, const uint16_t width, const uint16_t height) {
    uint16_t max_mip_text = std::bit_width(std::min(width, height));
    return std::min(true_mip, max_mip_text);
}
} // namespace texture

using namespace texture;

bool TextureCache::init(const bool hashless_texture_cache, size_t sampler_cache_size) {
    use_protect = hashless_texture_cache;

    // initialize the texture queue
    texture_queue.init(TextureCacheSize);
    // set the proper index of each entry
    for (size_t i = 0; i < TextureCacheSize; i++)
        texture_queue.items[i].content.index = static_cast<int>(i);

    // prevent stutter caused by the hashmap resizing
    texture_lookup.reserve(TextureCacheSize);

    use_sampler_cache = sampler_cache_size > 0;
    if (use_sampler_cache) {
        sampler_queue.init(sampler_cache_size);

        for (size_t i = 0; i < sampler_cache_size; i++)
            sampler_queue.items[i].content.index = static_cast<int>(i);

        sampler_lookup.reserve(sampler_cache_size);
    }

    return true;
}

void TextureCache::upload_texture(const SceGxmTexture &gxm_texture, MemState &mem) {
    R_PROFILE(__func__);

    bool is_vulkan = (backend == renderer::Backend::Vulkan);

    const SceGxmTextureFormat fmt = gxm::get_format(gxm_texture);
    const SceGxmTextureBaseFormat base_format = gxm::get_base_format(fmt);

    if (base_format == SCE_GXM_TEXTURE_BASE_FORMAT_YUV422) {
        LOG_ERROR_ONCE("Unimplemented YUV format {}, please report it to the developers.", log_hex(fmt::underlying(base_format)));
        return;
    }

    const bool is_block_compressed = gxm::is_block_compressed_format(base_format);
    uint32_t width = gxm::get_width(gxm_texture);
    uint32_t height = gxm::get_height(gxm_texture);

    const Ptr<uint8_t> data(gxm_texture.data_addr << 2);
    uint8_t *texture_data = data.get(mem);

    if (!texture_data) {
        return;
    }

    std::vector<uint8_t> texture_data_decompressed;
    std::vector<uint8_t> texture_pixels_lineared;

    const void *pixels = nullptr;

    uint32_t pixels_per_stride = 0;
    uint32_t bpp = gxm::bits_per_pixel(base_format);
    uint32_t bytes_per_pixel = (bpp + 7) >> 3;

    const auto texture_type = gxm_texture.texture_type();
    const bool is_swizzled = (texture_type == SCE_GXM_TEXTURE_SWIZZLED) || (texture_type == SCE_GXM_TEXTURE_CUBE) || (texture_type == SCE_GXM_TEXTURE_SWIZZLED_ARBITRARY) || (texture_type == SCE_GXM_TEXTURE_CUBE_ARBITRARY);
    // swizzled bcn textures
    const bool need_block_unswizzle = is_swizzled && gxm::is_bcn_format(base_format);

    uint32_t mip_index = 0;
    uint32_t total_mip = get_upload_mip(gxm_texture.true_mip_count(), width, height);
    uint32_t face_uploaded_count = 0;
    uint32_t face_total_count;
    uint32_t source_size = 0;
    uint32_t total_source_so_far = 0;

    // Modified during decompression
    const uint32_t org_width = width;
    const uint32_t org_height = height;

    uint32_t face_align_bytes = 4;

    // > 0 means texture cube
    int upload_type = 0;

    face_total_count = 1;
    if (texture_type == SCE_GXM_TEXTURE_CUBE || texture_type == SCE_GXM_TEXTURE_CUBE_ARBITRARY) {
        upload_type = 1;
        face_total_count = 6;

        if (gxm_texture.mip_count != 0xF) {
            const bool twok_align_cond1 = width >= 32 && height >= 32 && (bpp <= 8 || gxm::is_block_compressed_format(base_format));
            const bool twok_align_cond2 = width >= 16 && height >= 16 && (bpp == 16 || bpp == 32);
            const bool twok_align_cond3 = width >= 8 && height >= 8 && bpp == 64;

            if (twok_align_cond1 || twok_align_cond2 || twok_align_cond3) {
                face_align_bytes = 2048;
            }
        }
    }

    uint32_t layout_width;
    uint32_t layout_height;
    if (gxm_texture.mip_count == 0xF && texture_type == SCE_GXM_TEXTURE_LINEAR) {
        // a mipcount of 0xF means no mips, so for cube and planes, they follow each other directly without padding
        layout_width = width;
        layout_height = height;
    } else {
        layout_width = next_power_of_two(width);
        layout_height = next_power_of_two(height);
    }
    auto [block_width, block_height] = gxm::get_block_size(base_format);
    // block size in bytes
    const uint32_t block_size = (block_width * block_height * bpp) / 8;
    // from the number of pixels in a mipmap, we can get the number of blocks by shifting to the right by block_shift
    const uint32_t block_shift = std::bit_width(block_width * block_height) - 1;

    uint32_t align_width = block_width;
    uint32_t align_height = block_height;
    if (texture_type == SCE_GXM_TEXTURE_LINEAR) {
        align_width = std::max(align_width, 8U);
    } else if (texture_type == SCE_GXM_TEXTURE_TILED) {
        align_width = std::max(align_width, 32U);
        align_height = std::max(align_height, 32U);
    }

    const uint32_t org_layout_width = layout_width;
    const uint32_t org_layout_height = layout_height;

    while (face_uploaded_count < face_total_count && org_width > 0 && org_height > 0) {
        pixels = texture_data;

        SceGxmTextureBaseFormat upload_format = base_format;
        uint32_t memory_height = height;

        // Get pixels per stride
        pixels_per_stride = width;
        switch (texture_type) {
        case SCE_GXM_TEXTURE_SWIZZLED_ARBITRARY:
        case SCE_GXM_TEXTURE_CUBE_ARBITRARY:
            pixels_per_stride = next_power_of_two(width);
            memory_height = next_power_of_two(height);
            break;
        case SCE_GXM_TEXTURE_LINEAR_STRIDED:
            pixels_per_stride = gxm::get_stride_in_bytes(gxm_texture) / bytes_per_pixel;
            if (base_format == SCE_GXM_TEXTURE_BASE_FORMAT_P4) // P4 textures are the only one not byte aligned, therefore bytes_per_pixel should be 0.5 and not 1, correct it here
                pixels_per_stride *= 2;
            break;
        default:
            break;
        }
        pixels_per_stride = align(pixels_per_stride, align_width);
        memory_height = align(memory_height, align_height);

        // perform all needed conversions (formats not supported by modern GPUs)
        switch (base_format) {
        case SCE_GXM_TEXTURE_BASE_FORMAT_P4:
        case SCE_GXM_TEXTURE_BASE_FORMAT_P8:
            texture_data_decompressed.resize(pixels_per_stride * memory_height * 4);
            if (base_format == SCE_GXM_TEXTURE_BASE_FORMAT_P8) {
                palette_texture_to_rgba_8(reinterpret_cast<uint32_t *>(texture_data_decompressed.data()),
                    reinterpret_cast<const uint8_t *>(pixels), pixels_per_stride, memory_height, get_texture_palette(gxm_texture, mem));
            } else {
                palette_texture_to_rgba_4(reinterpret_cast<uint32_t *>(texture_data_decompressed.data()),
                    reinterpret_cast<const uint8_t *>(pixels), pixels_per_stride, memory_height, get_texture_palette(gxm_texture, mem));
            }
            pixels = texture_data_decompressed.data();
            bytes_per_pixel = 4;
            bpp = 32;
            upload_format = SCE_GXM_TEXTURE_BASE_FORMAT_U8U8U8U8;
            break;
        case SCE_GXM_TEXTURE_BASE_FORMAT_PVRT2BPP:
        case SCE_GXM_TEXTURE_BASE_FORMAT_PVRT4BPP:
        case SCE_GXM_TEXTURE_BASE_FORMAT_PVRTII2BPP:
        case SCE_GXM_TEXTURE_BASE_FORMAT_PVRTII4BPP:
            if (!is_swizzled)
                LOG_ERROR_ONCE("Unhandled non-swizzled PVRT format, please report it to the developers");

            texture_data_decompressed.resize(pixels_per_stride * memory_height * 4);
            // this actually also unswizzles the texture
            source_size = decompress_compressed_texture(base_format, texture_data_decompressed.data(), pixels, pixels_per_stride, memory_height);
            bytes_per_pixel = 4;
            bpp = 32;
            upload_format = SCE_GXM_TEXTURE_BASE_FORMAT_U8U8U8U8;
            pixels = texture_data_decompressed.data();
            break;
        case SCE_GXM_TEXTURE_BASE_FORMAT_U8U3U3U2:
            // Convert U8U3U3U2 to U8U8U8U8
            texture_data_decompressed.resize(pixels_per_stride * memory_height * 4);
            convert_U8U3U3U2_to_U8U8U8U8(texture_data_decompressed.data(), pixels, pixels_per_stride, memory_height);
            pixels = texture_data_decompressed.data();
            upload_format = SCE_GXM_TEXTURE_BASE_FORMAT_U8U8U8U8;
            bpp = 32;
            break;
        case SCE_GXM_TEXTURE_BASE_FORMAT_SE5M9M9M9:
            // this format is supported on all GPUs with vulkan
            if (is_vulkan)
                break;
            texture_data_decompressed.resize(pixels_per_stride * memory_height * 6);
            decompress_packed_float_e5m9m9m9(base_format, texture_data_decompressed.data(), pixels, width, memory_height);
            pixels = texture_data_decompressed.data();
            break;
        case SCE_GXM_TEXTURE_BASE_FORMAT_U2F10F10F10:
            // don't change what openGL is doing (which is completely wrong)
            if (!is_vulkan)
                break;
            texture_data_decompressed.resize(pixels_per_stride * memory_height * 8);
            convert_u2f10f10f10_to_f16f16f16f16(texture_data_decompressed.data(), pixels, pixels_per_stride, memory_height, fmt);
            pixels = texture_data_decompressed.data();
            upload_format = SCE_GXM_TEXTURE_BASE_FORMAT_F16F16F16F16;
            break;
        case SCE_GXM_TEXTURE_BASE_FORMAT_X8U24:
            texture_data_decompressed.resize(pixels_per_stride * memory_height * 4);
            if (is_vulkan) {
                // d24_u8 or x8_d24 is not supported on all GPUs (thanks AMD)
                convert_x8u24_to_f32(texture_data_decompressed.data(), pixels, pixels_per_stride, memory_height, fmt);
                upload_format = SCE_GXM_TEXTURE_BASE_FORMAT_F32;
            } else {
                // X8 = [24-31], D24 = [0-23], technically this is GL_UNSIGNED_INT_24_8_REV which does not exist
                // TODO: Requires shader to convert the normalized value read by GL to unsigned int. Just multiply by 2^24-1 when reading and you're done.
                // TODO: this is wrong, the depth is in the upper or lower 24 bits according to the swizzle
                convert_x8u24_to_u24x8(texture_data_decompressed.data(), pixels, pixels_per_stride, memory_height);
            }
            pixels = texture_data_decompressed.data();
            break;
        case SCE_GXM_TEXTURE_BASE_FORMAT_F32M:
            // Convert F32M to F32
            texture_data_decompressed.resize(pixels_per_stride * memory_height * 4);
            convert_f32m_to_f32(texture_data_decompressed.data(), pixels, pixels_per_stride, memory_height);
            pixels = texture_data_decompressed.data();
            upload_format = SCE_GXM_TEXTURE_BASE_FORMAT_F32;
            break;
        // TODO: we are decoding YUV420P2 as YUV420P3, that's completely wrong...
        // The reason I do not put an error message instead is because the video decoder
        // is always outputting YUV420P2 frames (instead of what is asked by the user...)
        // so this works in this case but that needs to be fixed
        case SCE_GXM_TEXTURE_BASE_FORMAT_YUV420P2:
        case SCE_GXM_TEXTURE_BASE_FORMAT_YUV420P3:
            texture_data_decompressed.resize(pixels_per_stride * memory_height * 4);
            yuv420P3_texture_to_rgb(texture_data_decompressed.data(),
                reinterpret_cast<const uint8_t *>(pixels), pixels_per_stride, memory_height, layout_width, layout_height);
            pixels = texture_data_decompressed.data();
            bpp = 32;
            upload_format = SCE_GXM_TEXTURE_BASE_FORMAT_U8U8U8U8;
            break;
        default:
            break;
        }

        if (texture_type != SCE_GXM_TEXTURE_LINEAR && texture_type != SCE_GXM_TEXTURE_LINEAR_STRIDED && !gxm::is_pvrt_format(base_format)) {
            // Convert data to linear layout
            texture_pixels_lineared.resize(pixels_per_stride * memory_height * bytes_per_pixel);

            if (is_swizzled && gxm::is_bcn_format(base_format))
                // just unswizzle the blocks
                resolve_z_order_compressed_texture(base_format, texture_pixels_lineared.data(), pixels, pixels_per_stride, memory_height);
            else if (is_swizzled)
                swizzled_texture_to_linear_texture(texture_pixels_lineared.data(), reinterpret_cast<const uint8_t *>(pixels), pixels_per_stride, memory_height,
                    static_cast<std::uint8_t>(bpp));
            else
                tiled_texture_to_linear_texture(texture_pixels_lineared.data(), reinterpret_cast<const uint8_t *>(pixels), pixels_per_stride, memory_height,
                    static_cast<std::uint8_t>(bpp));

            pixels = texture_pixels_lineared.data();
        }

        upload_texture_impl(upload_format, width, height, mip_index, pixels, upload_type, pixels_per_stride);

        const uint32_t nb_pixels = align(layout_width, align_width) * align(layout_height, align_height);
        const uint32_t mip_size = (nb_pixels >> block_shift) * block_size;
        texture_data += mip_size;
        total_source_so_far += mip_size;

        mip_index++;
        width /= 2;
        height /= 2;
        layout_width /= 2;
        layout_height /= 2;

        if (mip_index == total_mip) {
            if ((texture_type == SCE_GXM_TEXTURE_CUBE || texture_type == SCE_GXM_TEXTURE_CUBE_ARBITRARY) && gxm_texture.mip_count != 0xF) {
                // we must do as if all possible mips are here
                while (layout_width > 0 && layout_height > 0) {
                    const uint32_t nb_pixels = align(layout_width, align_width) * align(layout_height, align_height);
                    const uint32_t mip_size = (nb_pixels >> block_shift) * block_size;
                    texture_data += mip_size;
                    total_source_so_far += mip_size;
                    layout_width /= 2;
                    layout_height /= 2;
                }
            }

            mip_index = 0;
            face_uploaded_count++;

            layout_width = org_layout_width;
            layout_height = org_layout_height;
            width = org_width;
            height = org_height;

            upload_type++;

            uint32_t source_unaligned_size = total_source_so_far;
            total_source_so_far = align(total_source_so_far, face_align_bytes);

            texture_data += total_source_so_far - source_unaligned_size;
        }
    }
}

// remove everything related to the sampler state
static constexpr TextureGxmDataRepr default_texture_mask = {
    0x981E0000,
    0xFFFFFFFF,
    0xFFFFFFFC,
    0xF3FFFFFF
};
static constexpr TextureGxmDataRepr strided_texture_mask = {
    0x9FFE0E06,
    0xFFFFFFFF,
    0xFFFFFFFC,
    0xF3FFFFFF
};

void TextureCache::cache_and_bind_texture(const SceGxmTexture &gxm_texture, MemState &mem) {
    R_PROFILE(__func__);

    size_t index = 0;
    bool configure = false;
    bool upload = false;

    // Try to find GXM texture in cache.
    int cached_gxm_texture_index = -1;
    TextureGxmDataRepr texture_repr = std::bit_cast<TextureGxmDataRepr>(gxm_texture);
    if (use_sampler_cache) {
        // remove the sampler state from the representation
        const TextureGxmDataRepr &mask = (gxm_texture.texture_type() == SCE_GXM_TEXTURE_LINEAR_STRIDED) ? strided_texture_mask : default_texture_mask;
        for (int i = 0; i < 4; i++)
            texture_repr[i] &= mask[i];
    }
    auto gxm_it = texture_lookup.find(texture_repr);
    if (gxm_it != texture_lookup.end())
        // we found the texture in the cache
        cached_gxm_texture_index = gxm_it->second->index;

    Address range_protect_begin = 0;
    Address range_protect_end = 0;

    TextureCacheInfo *info;
    if (cached_gxm_texture_index == -1) {
        // Texture not found in cache.
        // get the least recently used texture, which info_list_head points to
        info = texture_queue.get_lru();
        index = info->index;
        if (info->timestamp > 0) {
            // Cache is full.
            LOG_DEBUG("Evicting texture {} (t = {}) from cache. Current t = {}.", index, info->timestamp, timestamp);
            texture_lookup.erase(info->texture);
        }
        texture_lookup[texture_repr] = info;

        configure = true;
        upload = true;
        // only hash the first mips, assume no game would modify other mips (and faces) without modifying the first one
        info->texture_size = gxm::texture_size_first_mip(gxm_texture);
        info->texture = texture_repr;

        // To prevent protecting too commonly accessed data that belongs to the page where the texture also resides
        // (for example, uniform buffer value and texture data got mixed, so page faults are triggered too many, it's not always good).
        // This works under the assumption that once this big enough texture decided to modify. It will have to modify either all of its data,
        // or replace with an entire new texture.
        bool should_use_hash = true;
        if (use_protect && info->texture_size >= mem.page_size * 4) {
            range_protect_begin = align(gxm_texture.data_addr << 2, mem.page_size);
            range_protect_end = align_down((gxm_texture.data_addr << 2) + info->texture_size, mem.page_size);

            if (range_protect_end - range_protect_begin >= mem.page_size * 4) {
                should_use_hash = false;
            }
        }

        info->use_hash = should_use_hash;
        if (info->use_hash) {
            info->hash = hash_texture_data(gxm_texture, info->texture_size, mem);
        }
    } else {
        // Texture is cached.
        index = cached_gxm_texture_index;
        info = gxm_it->second;
        configure = false;
        if (info->use_hash) {
            const uint64_t hash = hash_texture_data(gxm_texture, info->texture_size, mem);
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

    // set the texture as the mru
    texture_queue.set_as_mru(info);

    // retrieve the appropriate sampler if needed
    if (use_sampler_cache)
        cache_and_bind_sampler(gxm_texture);
}

int TextureCache::cache_and_bind_sampler(const SceGxmTexture &gxm_texture) {
    uint32_t compact_repr = 0;
    if (gxm_texture.texture_type() != SCE_GXM_TEXTURE_LINEAR_STRIDED) {
        compact_repr = 0b01
            | (gxm_texture.vaddr_mode << 2)
            | (gxm_texture.uaddr_mode << 5)
            | (gxm_texture.mip_filter << 8)
            | (gxm_texture.min_filter << 9)
            | (gxm_texture.mag_filter << 11)
            | (gxm_texture.lod_bias << 13)
            | (gxm_texture.lod_min0 << 19)
            | (gxm_texture.lod_min1 << 21);
    } else {
        // has a special representation
        compact_repr = 0b11
            | (gxm_texture.vaddr_mode << 2)
            | (gxm_texture.uaddr_mode << 5)
            | (gxm_texture.mag_filter << 8);
    }

    auto it = sampler_lookup.find(compact_repr);
    if (it != sampler_lookup.end()) {
        sampler_queue.set_as_mru(it->second);
        last_bound_sampler_index = it->second->index;
        return last_bound_sampler_index;
    }

    // we didn't find a matching sampler, create a new one
    SamplerCacheInfo *info = sampler_queue.get_lru();
    if (info->value != 0) {
        // the compact representation can never be 0, so we can erase the previous value
        sampler_lookup.erase(info->value);
    }

    sampler_queue.set_as_mru(info);
    sampler_lookup[compact_repr] = info;

    info->value = compact_repr;
    configure_sampler(info->index, gxm_texture);
    last_bound_sampler_index = info->index;
    return last_bound_sampler_index;
}

} // namespace renderer
