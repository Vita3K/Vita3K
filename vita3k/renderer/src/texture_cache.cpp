// Vita3K emulator project
// Copyright (C) 2021 Vita3K team
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
#include <renderer/pvrt-dec.h>
#include <renderer/texture_cache_state.h>

#include <gxm/functions.h>
#include <mem/ptr.h>
#include <util/align.h>
#include <util/log.h>

#include <algorithm> // find
#include <cstring> // memcmp
#include <numeric> // accumulate, reduce
#include <xxh3.h>
#ifdef WIN32
#include <execution>
#endif

namespace renderer {
namespace texture {

static TextureCacheHash hash_data(const void *data, size_t size) {
    auto hash = XXH_INLINE_XXH3_64bits(data, size);
    return TextureCacheHash(hash);
}

static TextureCacheHash hash_palette_data(const SceGxmTexture &texture, size_t count, const MemState &mem) {
    const uint32_t *const palette_bytes = get_texture_palette(texture, mem);
    const TextureCacheHash palette_hash = hash_data(palette_bytes, count * sizeof(uint32_t));
    return palette_hash;
}

TextureCacheHash hash_texture_data(const SceGxmTexture &texture, const MemState &mem) {
    R_PROFILE(__func__);
    const SceGxmTextureFormat format = gxm::get_format(&texture);
    const SceGxmTextureBaseFormat base_format = gxm::get_base_format(format);
    const size_t size = texture_size(texture);
    const Ptr<const void> data(texture.data_addr << 2);
    TextureCacheHash data_hash = 0;

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

static size_t find_lru(const TextureCacheState &cache, TextureCacheTimestamp current_time) {
    R_PROFILE(__func__);

    uint64_t oldest_age = current_time - cache.infoes.front().timestamp;
    size_t oldest_index = 0;

    for (size_t index = 1; index < cache.infoes.size(); ++index) {
        const uint64_t age = current_time - cache.infoes[index].timestamp;
        if (age > oldest_age) {
            oldest_age = age;
            oldest_index = index;
        }
    }

    return oldest_index;
}

bool can_texture_be_unswizzled_without_decode(SceGxmTextureBaseFormat fmt, bool is_vulkan) {
    return fmt == SCE_GXM_TEXTURE_BASE_FORMAT_P4
        || fmt == SCE_GXM_TEXTURE_BASE_FORMAT_U4U4U4U4
        || fmt == SCE_GXM_TEXTURE_BASE_FORMAT_P8
        || fmt == SCE_GXM_TEXTURE_BASE_FORMAT_U8
        || fmt == SCE_GXM_TEXTURE_BASE_FORMAT_U5U6U5
        || fmt == SCE_GXM_TEXTURE_BASE_FORMAT_U1U5U5U5
        || fmt == SCE_GXM_TEXTURE_BASE_FORMAT_U8U8U8
        || fmt == SCE_GXM_TEXTURE_BASE_FORMAT_U8U8U8U8
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

/**
 * \brief Try to resolve Z-order of block compressed texture
 *
 * \param fmt    Texture base format.
 * \param dest   Destination texture data. Size must be sufficient enough of align(width, 4) * align(height,4) * 4 (bytes).
 * \param data   Source data to solve.
 * \param width  Texture width.
 * \param height Texture height.
 *
 * \return Void.
 */
static void resolve_z_order_compressed_texture(SceGxmTextureBaseFormat fmt, void *dest, const void *data, const std::uint32_t width, const std::uint32_t height) {
    int bc_type = 0;

    switch (fmt) {
    case SCE_GXM_TEXTURE_BASE_FORMAT_UBC1:
        bc_type = 1;
        break;

    case SCE_GXM_TEXTURE_BASE_FORMAT_UBC2:
        bc_type = 2;
        break;

    case SCE_GXM_TEXTURE_BASE_FORMAT_UBC3:
        bc_type = 3;
        break;

    case SCE_GXM_TEXTURE_BASE_FORMAT_UBC4:
        bc_type = 4;
        break;

    case SCE_GXM_TEXTURE_BASE_FORMAT_SBC4:
        bc_type = 5;
        break;

    case SCE_GXM_TEXTURE_BASE_FORMAT_UBC5:
        bc_type = 6;
        break;

    case SCE_GXM_TEXTURE_BASE_FORMAT_SBC5:
        bc_type = 7;
        break;

    default:
        LOG_ERROR("Unknown compressed format {}", log_hex(fmt));
        break;
    }

    if (bc_type)
        renderer::texture::resolve_z_order_compressed_image(width, height, reinterpret_cast<const std::uint8_t *>(data),
            reinterpret_cast<std::uint8_t *>(dest), bc_type);
}

/**
 * \brief Try to decompress texture to 32-bit RGBA.
 *
 * \param fmt    Texture base format.
 * \param dest   Destination texture data. Size must be sufficient enough of align(width, 4) * align(height,4) * 4 (bytes).
 * \param data   Source data to decompress.
 * \param width  Texture width.
 * \param height Texture height.
 *
 * \return Size of source taken.
 */
static size_t decompress_compressed_swizz_texture(SceGxmTextureBaseFormat fmt, void *dest, const void *data, const std::uint32_t width, const std::uint32_t height) {
    int bc_type = 0;

    switch (fmt) {
    case SCE_GXM_TEXTURE_BASE_FORMAT_UBC1:
        bc_type = 1;
        break;

    case SCE_GXM_TEXTURE_BASE_FORMAT_UBC2:
        bc_type = 2;
        break;

    case SCE_GXM_TEXTURE_BASE_FORMAT_UBC3:
        bc_type = 3;
        break;

    case SCE_GXM_TEXTURE_BASE_FORMAT_UBC4:
        bc_type = 4;
        break;

    case SCE_GXM_TEXTURE_BASE_FORMAT_SBC4:
        bc_type = 5;
        break;

    case SCE_GXM_TEXTURE_BASE_FORMAT_UBC5:
        bc_type = 6;
        break;

    case SCE_GXM_TEXTURE_BASE_FORMAT_SBC5:
        bc_type = 7;
        break;

    default:
        break;
    }

    if (bc_type) {
        decompress_bc_swizz_image(width, height, reinterpret_cast<const std::uint8_t *>(data),
            reinterpret_cast<std::uint32_t *>(dest), bc_type);
        return (((width + 3) / 4) * ((height + 3) / 4) * ((bc_type != 1 && bc_type != 4 && bc_type != 5) ? 16 : 8));
    } else if ((fmt >= SCE_GXM_TEXTURE_BASE_FORMAT_PVRT2BPP) && (fmt <= SCE_GXM_TEXTURE_BASE_FORMAT_PVRTII4BPP)) {
        pvr::PVRTDecompressPVRTC(data, (fmt == SCE_GXM_TEXTURE_BASE_FORMAT_PVRT2BPP) || (fmt == SCE_GXM_TEXTURE_BASE_FORMAT_PVRTII2BPP), width, height,
            (fmt == SCE_GXM_TEXTURE_BASE_FORMAT_PVRTII2BPP) || (fmt == SCE_GXM_TEXTURE_BASE_FORMAT_PVRTII4BPP), reinterpret_cast<uint8_t *>(dest));

        const bool is_2bpp = (fmt == SCE_GXM_TEXTURE_BASE_FORMAT_PVRT2BPP) || (fmt == SCE_GXM_TEXTURE_BASE_FORMAT_PVRTII2BPP);

        const std::uint32_t num_xword = (width + (is_2bpp ? 7 : 3)) / (is_2bpp ? 8 : 4);
        const std::uint32_t num_yword = (height + 3) / 4;

        return (size_t)num_xword * (size_t)num_yword * 8;
    }

    return 0;
}

/**
 * \brief Try to decompress texture to 16-bit RGB floating point color.
 *
 * \param fmt    Texture base format.
 * \param dest   Destination texture data. Size must be sufficient enough of align(width, 4) * height * 4 (bytes).
 * \param data   Source data to decompress.
 * \param width  Texture width.
 * \param height Texture height.
 *
 * \return Void.
 */
static void decompress_packed_float_e5m9m9m9(SceGxmTextureBaseFormat fmt, void *dest, const void *data, const uint32_t width, const uint32_t height) {
    const uint32_t *in = reinterpret_cast<const uint32_t *>(data);
    uint16_t *out = reinterpret_cast<uint16_t *>(dest);

    for (uint32_t in_offset = 0, out_offset = 0; in_offset < width * height; ++in_offset) {
        const uint32_t packed = in[in_offset];
        const uint16_t exponent = static_cast<uint16_t>(packed >> 17);

        out[out_offset++] = exponent | ((packed & (0x1FF << 18)) >> 17);
        out[out_offset++] = exponent | ((packed & (0x1FF << 9)) >> 8);
        out[out_offset++] = exponent | ((packed & 0x1FF) << 1);
    }
}

static void convert_x8u24_to_u24x8(void *dest, const void *data, const uint32_t width, const uint32_t height, const size_t row_length_in_pixels) {
    auto dst = static_cast<uint32_t *>(dest);
    auto src = static_cast<const uint32_t *>(data);

    for (uint32_t row = 0; row < height; ++row) {
        for (uint32_t col = 0; col < width; ++col) {
            const uint32_t src_value = src[col];
            const uint32_t value = (src_value << 8) | (src_value >> 24);
            *dst++ = value;
        }

        src += row_length_in_pixels;
    }
}

// Convert x8u24 (or u24x8) format to f32 (only keep the u24 part)
// Do not use a depth-stencil format as x8d24 is not supported on all GPUs for Vulkan
static void convert_x8u24_to_f32(void *dest, const void *data, const uint32_t width, const uint32_t height, const size_t row_length_in_pixels, const SceGxmTextureFormat format) {
    const SceGxmTextureSwizzle2ModeAlt swizzle = static_cast<SceGxmTextureSwizzle2ModeAlt>(format & SCE_GXM_TEXTURE_SWIZZLE_MASK);
    // is the depth in the upper or lower 24 bits of the data?
    int shift_amount = (swizzle == SCE_GXM_TEXTURE_SWIZZLE2_DS) ? 8 : 0;
    auto dst = static_cast<float *>(dest);
    auto src = static_cast<const uint32_t *>(data);

    for (uint32_t row = 0; row < height; ++row) {
        for (uint32_t col = 0; col < width; ++col) {
            const uint32_t src_value = src[col];
            const uint32_t d24 = (src_value >> shift_amount) & ((1U << 24) - 1);
            *dst++ = static_cast<float>(d24) / ((1U << 24) - 1);
        }

        src += row_length_in_pixels;
    }
}

static void convert_f32m_to_f32(void *dest, const void *data, const uint32_t width, const uint32_t height, const size_t row_length_in_pixels) {
    auto dst = static_cast<uint32_t *>(dest);
    auto src = static_cast<const uint32_t *>(data);

    for (uint32_t row = 0; row < height; ++row) {
        for (uint32_t col = 0; col < width; ++col) {
            const uint32_t src_value = src[col];
            const uint32_t value = src_value & 0x7FFFFFFF;
            *dst++ = value;
        }

        src += row_length_in_pixels;
    }
}

static uint16_t f10_to_f16(const uint16_t f10) {
    // f16 has a 10 bit mantissa and a 5 bit exponent
    // f10 has a 5 bit mantissa and a 5 bit exponent
    // so we just need to put the exponent in the right location, add zeros to the mantissa
    // and it should work
    const uint16_t exponent = (f10 >> 5) & 0b11111;
    const uint16_t mantissa = f10 & 0b11111;
    const uint16_t f16 = (exponent << 10) | (mantissa << 5);
    return f16;
}

static void convert_u2f10f10f10_to_f16f16f16f16(void *dest, const void *data, const uint32_t width, const uint32_t height, const size_t row_length_in_pixels, const SceGxmTextureFormat format) {
    auto dst = static_cast<std::array<uint16_t, 4> *>(dest);
    auto src = static_cast<const uint32_t *>(data);

    // are the 2 alpha bits in the upper or lower bits of the pixel ?
    bool is_alpha_upper = (format == SCE_GXM_TEXTURE_FORMAT_U2F10F10F10_ABGR
        || format == SCE_GXM_TEXTURE_FORMAT_U2F10F10F10_ARGB
        || format == SCE_GXM_TEXTURE_FORMAT_X2F10F10F10_1BGR
        || format == SCE_GXM_TEXTURE_FORMAT_X2F10F10F10_1RGB);

    for (uint32_t row = 0; row < height; ++row) {
        for (uint32_t col = 0; col < width; ++col) {
            uint32_t src_value = src[col];
            uint16_t f1, f2, f3;
            int dst_idx;
            // first get the 2 alpha bits
            if (is_alpha_upper) {
                (*dst)[3] = (src_value >> 30) / 3.0f;
                dst_idx = 0;
            } else {
                (*dst)[0] = (src_value & 0b11) / 3.0f;
                dst_idx = 1;
                src_value >>= 2;
            }

            // decode the 3 rgb components
            for (int i = 0; i < 3; i++) {
                const uint16_t comp = src_value & ((1 << 10) - 1);
                src_value >>= 10;
                (*dst)[dst_idx++] = f10_to_f16(comp);
            }
            dst++;
        }

        src += row_length_in_pixels;
    }
}

/**
 * \brief Remove arbitraty blocks from block compressed texture
 *
 * \param fmt    Texture base format.
 * \param dest   Destination texture data. Size must be sufficient enough of ((width + 3) / 4) * ((height + 3) / 4) * 8 or 16 (bytes) depending on texture base format.
 * \param data   Source data to decompress.
 * \param width  Texture width.
 * \param height Texture height.
 *
 * \return Void.
 */
static void remove_compressed_arbitrary_blocks(SceGxmTextureBaseFormat fmt, void *dest, const void *data, const std::uint32_t width, const std::uint32_t height) {
    uint32_t w = (width + 3) / 4;
    uint32_t h = (height + 3) / 4;

    uint32_t a_w = next_power_of_two(w);

    std::size_t block_size;
    switch (fmt) {
    case SCE_GXM_TEXTURE_BASE_FORMAT_UBC1:
    case SCE_GXM_TEXTURE_BASE_FORMAT_UBC4:
    case SCE_GXM_TEXTURE_BASE_FORMAT_SBC4:
        block_size = 8;
        return;
    case SCE_GXM_TEXTURE_BASE_FORMAT_UBC2:
    case SCE_GXM_TEXTURE_BASE_FORMAT_UBC3:
    case SCE_GXM_TEXTURE_BASE_FORMAT_UBC5:
    case SCE_GXM_TEXTURE_BASE_FORMAT_SBC5:
        block_size = 16;
        break;
    default:
        return;
    }

    w *= block_size;
    a_w *= block_size;

    const std::uint8_t *src = reinterpret_cast<const std::uint8_t *>(data);
    std::uint8_t *dst = reinterpret_cast<std::uint8_t *>(dest);

    for (std::size_t j = h; j; j--) {
        memcpy(dst, src, w);
        src += a_w;
        dst += w;
    }
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

void upload_bound_texture(const TextureCacheState &cache, const SceGxmTexture &gxm_texture, const MemState &mem) {
    R_PROFILE(__func__);

    bool is_vulkan = (*cache.backend == Backend::Vulkan);

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
    size_t bpp = renderer::texture::bits_per_pixel(base_format);
    size_t bytes_per_pixel = (bpp + 7) >> 3;

    const auto texture_type = gxm_texture.texture_type();
    const bool is_swizzled = (texture_type == SCE_GXM_TEXTURE_SWIZZLED) || (texture_type == SCE_GXM_TEXTURE_CUBE) || (texture_type == SCE_GXM_TEXTURE_SWIZZLED_ARBITRARY) || (texture_type == SCE_GXM_TEXTURE_CUBE_ARBITRARY);
    const bool need_unswizzle = is_swizzled && block_compressed;
    const bool need_decompress_and_unswizzle_on_cpu = is_swizzled && !block_compressed && !can_texture_be_unswizzled_without_decode(base_format, is_vulkan);

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

        const bool twok_align_cond1 = ((width >= 32) && (height >= 32) && ((bytes_per_pixel == 1) || (is_block_compressed_format(base_format))));
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
                yuv_texture_pixels.resize(width * height * 3);
                renderer::texture::yuv420_texture_to_rgb(yuv_texture_pixels.data(),
                    reinterpret_cast<const uint8_t *>(pixels), width, height);
                pixels = yuv_texture_pixels.data();
                pixels_per_stride = width;
                bpp = 24;
                upload_format = SCE_GXM_TEXTURE_BASE_FORMAT_U8U8U8;
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

        cache.upload_texture_callback(upload_format, width, height, mip_index, pixels, upload_type, block_compressed, pixels_per_stride);

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

void cache_and_bind_texture(TextureCacheState &cache, const SceGxmTexture &gxm_texture, MemState &mem) {
    R_PROFILE(__func__);

    size_t index = 0;
    bool configure = false;
    bool upload = false;
    const size_t size = texture_size(gxm_texture);

    // Try to find GXM texture in cache.
    int cached_gxm_texture_index = -1;
    for (size_t a = 0; a < cache.used; a++) {
        if (memcmp(&cache.infoes[a].texture, &gxm_texture, sizeof(SceGxmTexture)) == 0) {
            cached_gxm_texture_index = a;
            break;
        }
    }

    Address range_protect_begin = 0;
    Address range_protect_end = 0;
    bool should_use_hash = true;

    // To prevent protecting too commonly accessed data that belongs to the page where the texture also resides
    // (for example, uniform buffer value and texture data got mixed, so page faults are triggered too many, it's not always good).
    // This works under the assumption that once this big enough texture decided to modify. It will have to modify either all of its data,
    // or replace with an entire new texture.
    if (cache.use_protect && size >= mem.page_size * 4) {
        range_protect_begin = align(gxm_texture.data_addr << 2, mem.page_size);
        range_protect_end = align_down((gxm_texture.data_addr << 2) + size, mem.page_size);

        if (range_protect_end - range_protect_begin >= mem.page_size * 4) {
            should_use_hash = false;
        }
    }

    TextureCacheInfo *info;
    if (cached_gxm_texture_index == -1) {
        // Texture not found in cache.
        if (cache.used < TextureCacheSize) {
            // Cache is not full. Add texture to cache.
            index = cache.used;
            ++cache.used;
        } else {
            // Cache is full. Find least recently used texture.
            index = find_lru(cache, cache.timestamp);
            LOG_DEBUG("Evicting texture {} (t = {}) from cache. Current t = {}.", index, cache.infoes[index].timestamp, cache.timestamp);
        }
        configure = true;
        upload = true;
        cache.infoes[index] = TextureCacheInfo(gxm_texture);
        info = &cache.infoes[index];
        info->use_hash = should_use_hash;
        if (info->use_hash) {
            info->hash = hash_texture_data(gxm_texture, mem);
        }
    } else {
        // Texture is cached.
        index = cached_gxm_texture_index;
        info = &cache.infoes[index];
        configure = false;
        if (info->use_hash) {
            const TextureCacheHash hash = hash_texture_data(gxm_texture, mem);
            upload = info->hash != hash;
            info->hash = hash;
        } else {
            upload = info->dirty;
        }
    }

    if (gxm_texture.data_addr == 0) {
        upload = false;
    }

// Fix memory access error in the condition check for texture cache method
// (hashed vs hashless) in Clang compilers due to compiler optimizations
#ifdef __clang__
    if (!info->use_hash) {
        std::cout << "";
    }
#endif

    cache.select_callback(index, &gxm_texture);

    if (configure) {
        cache.configure_texture_callback(cache, &gxm_texture);
    }
    if (upload) {
        upload_bound_texture(cache, gxm_texture, mem);
        if (!info->use_hash) {
            info->dirty = false;
            add_protect(mem, range_protect_begin, range_protect_end - range_protect_begin, MEM_PERM_READONLY, [&cache, info, gxm_texture](Address, bool) {
                if (memcmp(&info->texture, &gxm_texture, sizeof(SceGxmTexture)) == 0) {
                    info->dirty = true;
                }

                return true;
            });
        }
    }

    info->timestamp = cache.timestamp++;
}

} // namespace texture
} // namespace renderer
