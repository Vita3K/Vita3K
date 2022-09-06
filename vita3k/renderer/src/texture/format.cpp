// Vita3K emulator project
// Copyright (C) 2025 Vita3K team
// Copyright (c) 2009 Benjamin Dobell, Glass Echidna
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

#include <cmath>
#include <cstdint>
#include <cstring>

#include <gxm/types.h>
#include <renderer/functions.h>
#include <renderer/pvrt-dec.h>
#include <util/log.h>

namespace renderer::texture {

bool convert_base_texture_format_to_base_color_format(SceGxmTextureBaseFormat format, SceGxmColorBaseFormat &color_format) {
    static const std::map<std::uint32_t, std::uint32_t> TEXTURE_TO_COLOR_FORMAT_MAPPING = {
        { SCE_GXM_TEXTURE_BASE_FORMAT_U8U8U8U8, SCE_GXM_COLOR_BASE_FORMAT_U8U8U8U8 },
        { SCE_GXM_TEXTURE_BASE_FORMAT_U8U8U8, SCE_GXM_COLOR_BASE_FORMAT_U8U8U8 },
        { SCE_GXM_TEXTURE_BASE_FORMAT_U5U6U5, SCE_GXM_COLOR_BASE_FORMAT_U5U6U5 },
        { SCE_GXM_TEXTURE_BASE_FORMAT_U1U5U5U5, SCE_GXM_COLOR_BASE_FORMAT_U1U5U5U5 },
        { SCE_GXM_TEXTURE_BASE_FORMAT_U4U4U4U4, SCE_GXM_COLOR_BASE_FORMAT_U4U4U4U4 },
        { SCE_GXM_TEXTURE_BASE_FORMAT_U8U3U3U2, SCE_GXM_COLOR_BASE_FORMAT_U8U3U3U2 },
        { SCE_GXM_TEXTURE_BASE_FORMAT_F16, SCE_GXM_COLOR_BASE_FORMAT_F16 },
        { SCE_GXM_TEXTURE_BASE_FORMAT_F16F16, SCE_GXM_COLOR_BASE_FORMAT_F16F16 },
        { SCE_GXM_TEXTURE_BASE_FORMAT_F32, SCE_GXM_COLOR_BASE_FORMAT_F32 },
        { SCE_GXM_TEXTURE_BASE_FORMAT_S16, SCE_GXM_COLOR_BASE_FORMAT_S16 },
        { SCE_GXM_TEXTURE_BASE_FORMAT_S16S16, SCE_GXM_COLOR_BASE_FORMAT_S16S16 },
        { SCE_GXM_TEXTURE_BASE_FORMAT_U16, SCE_GXM_COLOR_BASE_FORMAT_U16 },
        { SCE_GXM_TEXTURE_BASE_FORMAT_U16U16, SCE_GXM_COLOR_BASE_FORMAT_U16U16 },
        { SCE_GXM_TEXTURE_BASE_FORMAT_U2U10U10U10, SCE_GXM_COLOR_BASE_FORMAT_U2U10U10U10 },
        { SCE_GXM_TEXTURE_BASE_FORMAT_U8, SCE_GXM_COLOR_BASE_FORMAT_U8 },
        { SCE_GXM_TEXTURE_BASE_FORMAT_S8, SCE_GXM_COLOR_BASE_FORMAT_S8 },
        { SCE_GXM_TEXTURE_BASE_FORMAT_S5S5U6, SCE_GXM_COLOR_BASE_FORMAT_S5S5U6 },
        { SCE_GXM_TEXTURE_BASE_FORMAT_U8U8, SCE_GXM_COLOR_BASE_FORMAT_U8U8 },
        { SCE_GXM_TEXTURE_BASE_FORMAT_S8S8, SCE_GXM_COLOR_BASE_FORMAT_S8S8 },
        { SCE_GXM_TEXTURE_BASE_FORMAT_S8S8S8S8, SCE_GXM_COLOR_BASE_FORMAT_S8S8S8S8 },
        { SCE_GXM_TEXTURE_BASE_FORMAT_F16F16F16F16, SCE_GXM_COLOR_BASE_FORMAT_F16F16F16F16 },
        { SCE_GXM_TEXTURE_BASE_FORMAT_F32F32, SCE_GXM_COLOR_BASE_FORMAT_F32F32 },
        { SCE_GXM_TEXTURE_BASE_FORMAT_F11F11F10, SCE_GXM_COLOR_BASE_FORMAT_F11F11F10 },
        { SCE_GXM_TEXTURE_BASE_FORMAT_SE5M9M9M9, SCE_GXM_COLOR_BASE_FORMAT_SE5M9M9M9 },
        { SCE_GXM_TEXTURE_BASE_FORMAT_U2F10F10F10, SCE_GXM_COLOR_BASE_FORMAT_U2F10F10F10 },
        { SCE_GXM_TEXTURE_BASE_FORMAT_U32U32, SCE_GXM_COLOR_BASE_FORMAT_F32F32 }
    };

    auto ite = TEXTURE_TO_COLOR_FORMAT_MAPPING.find(format);
    if (ite == TEXTURE_TO_COLOR_FORMAT_MAPPING.end())
        return false;

    color_format = static_cast<SceGxmColorBaseFormat>(ite->second);
    return true;
}

SceGxmTextureBaseFormat get_matching_decompressed_format(SceGxmTextureBaseFormat fmt) {
    switch (fmt) {
    case SCE_GXM_TEXTURE_BASE_FORMAT_UBC4:
        return SCE_GXM_TEXTURE_BASE_FORMAT_U8;
    case SCE_GXM_TEXTURE_BASE_FORMAT_SBC4:
        return SCE_GXM_TEXTURE_BASE_FORMAT_S8;
    case SCE_GXM_TEXTURE_BASE_FORMAT_UBC5:
        return SCE_GXM_TEXTURE_BASE_FORMAT_U8U8;
    case SCE_GXM_TEXTURE_BASE_FORMAT_SBC5:
        return SCE_GXM_TEXTURE_BASE_FORMAT_S8S8;
    default:
        // BC1/2/3
        return SCE_GXM_TEXTURE_BASE_FORMAT_U8U8U8U8;
    }
}

bool is_astc_format(SceGxmTextureBaseFormat base_format) {
    const uint32_t fmt = static_cast<uint32_t>(base_format);
    return fmt >= static_cast<uint32_t>(SCE_GXM_TEXTURE_BASE_FORMAT_ASTC4x4)
        && fmt <= static_cast<uint32_t>(SCE_GXM_TEXTURE_BASE_FORMAT_ASTC12x12);
}

void resolve_z_order_compressed_texture(SceGxmTextureBaseFormat fmt, void *dest, const void *data, const uint32_t width, const uint32_t height) {
    uint32_t block_size = 0;

    switch (fmt) {
    case SCE_GXM_TEXTURE_BASE_FORMAT_UBC1:
    case SCE_GXM_TEXTURE_BASE_FORMAT_UBC4:
    case SCE_GXM_TEXTURE_BASE_FORMAT_SBC4:
        block_size = 8;
        break;

    case SCE_GXM_TEXTURE_BASE_FORMAT_UBC2:
    case SCE_GXM_TEXTURE_BASE_FORMAT_UBC3:
    case SCE_GXM_TEXTURE_BASE_FORMAT_UBC5:
    case SCE_GXM_TEXTURE_BASE_FORMAT_SBC5:
        block_size = 16;
        break;

    default:
        LOG_ERROR("Unknown compressed format {}", log_hex(fmt));
        break;
    }

    if (block_size > 0)
        resolve_z_order_compressed_image(width, height, static_cast<const std::uint8_t *>(data),
            static_cast<std::uint8_t *>(dest), block_size);
}

uint32_t decompress_compressed_texture(SceGxmTextureBaseFormat fmt, void *dest, const void *data, const uint32_t width, const uint32_t height) {
    uint8_t format_id = 0;

    switch (fmt) {
    case SCE_GXM_TEXTURE_BASE_FORMAT_UBC1:
        format_id = 1;
        break;

    case SCE_GXM_TEXTURE_BASE_FORMAT_UBC2:
        format_id = 2;
        break;

    case SCE_GXM_TEXTURE_BASE_FORMAT_UBC3:
        format_id = 3;
        break;

    case SCE_GXM_TEXTURE_BASE_FORMAT_UBC4:
        format_id = 4;
        break;

    case SCE_GXM_TEXTURE_BASE_FORMAT_SBC4:
        format_id = 5;
        break;

    case SCE_GXM_TEXTURE_BASE_FORMAT_UBC5:
        format_id = 6;
        break;

    case SCE_GXM_TEXTURE_BASE_FORMAT_SBC5:
        format_id = 7;
        break;

    default:
        break;
    }

    if (format_id) {
        decompress_bc_image(width, height, static_cast<const uint8_t *>(data),
            static_cast<uint32_t *>(dest), format_id);
        return (((width + 3) / 4) * ((height + 3) / 4) * ((format_id != 1 && format_id != 4 && format_id != 5) ? 16 : 8));
    } else if ((fmt >= SCE_GXM_TEXTURE_BASE_FORMAT_PVRT2BPP) && (fmt <= SCE_GXM_TEXTURE_BASE_FORMAT_PVRTII4BPP)) {
        pvr::PVRTDecompressPVRTC(data, (fmt == SCE_GXM_TEXTURE_BASE_FORMAT_PVRT2BPP) || (fmt == SCE_GXM_TEXTURE_BASE_FORMAT_PVRTII2BPP), width, height,
            (fmt == SCE_GXM_TEXTURE_BASE_FORMAT_PVRTII2BPP) || (fmt == SCE_GXM_TEXTURE_BASE_FORMAT_PVRTII4BPP), static_cast<uint8_t *>(dest));

        const bool is_2bpp = (fmt == SCE_GXM_TEXTURE_BASE_FORMAT_PVRT2BPP) || (fmt == SCE_GXM_TEXTURE_BASE_FORMAT_PVRTII2BPP);

        const uint32_t num_xword = (width + (is_2bpp ? 7 : 3)) / (is_2bpp ? 8 : 4);
        const uint32_t num_yword = (height + 3) / 4;

        return num_xword * num_yword * 8;
    } else {
        LOG_ERROR("Trying to decompress and unswizzle unknown format {}", log_hex(fmt));
    }

    return 0;
}

void decompress_packed_float_e5m9m9m9(SceGxmTextureBaseFormat fmt, void *dest, const void *data, const uint32_t width, const uint32_t height) {
    const uint32_t *in = static_cast<const uint32_t *>(data);
    uint16_t *out = static_cast<uint16_t *>(dest);

    for (uint32_t in_offset = 0, out_offset = 0; in_offset < width * height; ++in_offset) {
        const uint32_t packed = in[in_offset];
        const uint16_t exponent = static_cast<uint16_t>(packed >> 17);

        out[out_offset++] = exponent | ((packed & (0x1FF << 18)) >> 17);
        out[out_offset++] = exponent | ((packed & (0x1FF << 9)) >> 8);
        out[out_offset++] = exponent | ((packed & 0x1FF) << 1);
    }
}

void convert_x8u24_to_u24x8(void *dest, const void *data, const uint32_t width, const uint32_t height) {
    auto dst = static_cast<uint32_t *>(dest);
    auto src = static_cast<const uint32_t *>(data);

    for (uint32_t row = 0; row < height; ++row) {
        for (uint32_t col = 0; col < width; ++col) {
            uint32_t src_value = src[row * width + col];
            dst[row * width + col] = (src_value << 8) | (src_value >> 24);
        }
    }
}

void convert_x8u24_to_f32(void *dest, const void *data, const uint32_t width, const uint32_t height, const SceGxmTextureFormat format) {
    const SceGxmTextureSwizzle2ModeAlt swizzle = static_cast<SceGxmTextureSwizzle2ModeAlt>(format & SCE_GXM_TEXTURE_SWIZZLE_MASK);
    // is the depth in the upper or lower 24 bits of the data?
    int shift_amount = (swizzle == SCE_GXM_TEXTURE_SWIZZLE2_DS) ? 8 : 0;
    auto dst = static_cast<float *>(dest);
    auto src = static_cast<const uint32_t *>(data);

    for (uint32_t row = 0; row < height; ++row) {
        for (uint32_t col = 0; col < width; ++col) {
            const uint32_t src_value = src[row * width + col];
            const uint32_t d24 = (src_value >> shift_amount) & ((1U << 24) - 1);
            dst[row * width + col] = static_cast<float>(d24) / ((1U << 24) - 1);
        }
    }
}

void convert_U8U3U3U2_to_U8U8U8U8(void *dest, const void *data, const uint32_t width, const uint32_t height) {
    auto dst = static_cast<uint32_t *>(dest);
    auto src = static_cast<const uint16_t *>(data);

    for (uint32_t row = 0; row < height; ++row) {
        for (uint32_t col = 0; col < width; ++col) {
            const uint32_t src_value = src[row * width + col];

            const uint8_t alpha = (src_value & 0xFF00) >> 8;
            const uint8_t red = (src_value & 0x00E0) >> 5;
            const uint8_t green = (src_value & 0x001C) >> 2;
            const uint8_t blue = (src_value & 0x0003);

            const uint32_t value = (alpha << 24) | (blue << 22) | (blue << 20) | (blue << 18) | (blue << 16) | (green << 13) | (green << 10) | ((green & 0b110) << 7) | (red << 5) | (red << 2) | (red >> 1);

            dst[row * width + col] = value;
        }
    }
}

void convert_f32m_to_f32(void *dest, const void *data, const uint32_t width, const uint32_t height) {
    auto dst = static_cast<uint32_t *>(dest);
    auto src = static_cast<const uint32_t *>(data);

    for (uint32_t row = 0; row < height; ++row) {
        for (uint32_t col = 0; col < width; ++col) {
            const uint32_t src_value = src[row * width + col];
            dst[row * width + col] = src_value & 0x7FFFFFFF;
        }
    }
}

static uint16_t f10_to_f16(const uint16_t f10) {
    // f16 has a 10 bit mantissa and a 5 bit exponent
    // f10 has a 5 bit mantissa and a 5 bit exponent
    // so we just need to put the exponent in the right location, add zeros to the mantissa
    // and it should work
    // Note: I don't think this works for subnormal numbers
    const uint16_t exponent = (f10 >> 5) & 0b11111;
    const uint16_t mantissa = f10 & 0b11111;
    const uint16_t f16 = (exponent << 10) | (mantissa << 5);
    return f16;
}

void convert_u2f10f10f10_to_f16f16f16f16(void *dest, const void *data, const uint32_t width, const uint32_t height, const SceGxmTextureFormat format) {
    auto dst = static_cast<std::array<uint16_t, 4> *>(dest);
    auto src = static_cast<const uint32_t *>(data);

    // are the 2 alpha bits in the upper or lower bits of the pixel ?
    bool is_alpha_upper = (format == SCE_GXM_TEXTURE_FORMAT_U2F10F10F10_ABGR
        || format == SCE_GXM_TEXTURE_FORMAT_U2F10F10F10_ARGB
        || format == SCE_GXM_TEXTURE_FORMAT_X2F10F10F10_1BGR
        || format == SCE_GXM_TEXTURE_FORMAT_X2F10F10F10_1RGB);

    // f16 values for u2 channel
    constexpr uint16_t u2_to_f16[4] = { 0, 0x3555, 0x3955, 0x3c00 }; /*{0,1/3,2/3,1}*/

    for (uint32_t row = 0; row < height; ++row) {
        for (uint32_t col = 0; col < width; ++col) {
            uint32_t src_value = src[row * width + col];
            int dst_idx;
            // first get the 2 alpha bits
            if (is_alpha_upper) {
                dst[row * width + col][3] = u2_to_f16[(src_value >> 30)];
                dst_idx = 0;
            } else {
                dst[row * width + col][0] = u2_to_f16[(src_value & 0b11)];
                dst_idx = 1;
                src_value >>= 2;
            }

            // decode the 3 rgb components
            for (int i = 0; i < 3; i++) {
                const uint16_t comp = src_value & ((1 << 10) - 1);
                src_value >>= 10;
                dst[row * width + col][dst_idx++] = f10_to_f16(comp);
            }
        }
    }
}

// Based on this: http://xen.firefly.nu/up/rearrange.c.html
// https://fgiesen.wordpress.com/2009/12/13/decoding-morton-codes/
// Thanks daniel from GXTConvert finding this out first

// Inverse of Part1By1 - "delete" all odd-indexed bits
static uint32_t compact_one_by_one(uint32_t x) {
    x &= 0x55555555; // x = -f-e -d-c -b-a -9-8 -7-6 -5-4 -3-2 -1-0
    x = (x ^ (x >> 1)) & 0x33333333; // x = --fe --dc --ba --98 --76 --54 --32 --10
    x = (x ^ (x >> 2)) & 0x0f0f0f0f; // x = ---- fedc ---- ba98 ---- 7654 ---- 3210
    x = (x ^ (x >> 4)) & 0x00ff00ff; // x = ---- ---- fedc ba98 ---- ---- 7654 3210
    x = (x ^ (x >> 8)) & 0x0000ffff; // x = ---- ---- ---- ---- fedc ba98 7654 3210
    return x;
}

uint32_t decode_morton2_y(uint32_t code) {
    return compact_one_by_one(code >> 0);
}

uint32_t decode_morton2_x(uint32_t code) {
    return compact_one_by_one(code >> 1);
}

static uint32_t Part1By1(uint32_t x) {
    x &= 0x0000ffff; // x = ---- ---- ---- ---- fedc ba98 7654 3210
    x = (x ^ (x << 8)) & 0x00ff00ff; // x = ---- ---- fedc ba98 ---- ---- 7654 3210
    x = (x ^ (x << 4)) & 0x0f0f0f0f; // x = ---- fedc ---- ba98 ---- 7654 ---- 3210
    x = (x ^ (x << 2)) & 0x33333333; // x = --fe --dc --ba --98 --76 --54 --32 --10
    x = (x ^ (x << 1)) & 0x55555555; // x = -f-e -d-c -b-a -9-8 -7-6 -5-4 -3-2 -1-0
    return x;
}

uint32_t encode_morton(uint16_t x, uint16_t y, uint16_t width, uint16_t height) {
    assert((width & (width - 1)) == 0);
    assert((height & (height - 1)) == 0);

    // assuming width < height:
    // xxxxxxx yyyy
    uint16_t min = std::min(width, height);
    uint32_t k = std::bit_width(min) - 1;
    // upper unswizzled bits
    // xxx--------
    uint32_t result = static_cast<uint32_t>((x >> k) | (y >> k)) << 2 * k;
    // xxxx-x-x-x-
    result |= (Part1By1(x & (min - 1)) << 1);
    // xxxxyxyxyxy
    result |= Part1By1(y & (min - 1));
    return result;
}

void swizzled_texture_to_linear_texture(uint8_t *dest, const uint8_t *src, uint16_t width, uint16_t height, uint8_t bits_per_pixel) {
    if (bits_per_pixel % 8 != 0) {
        // Don't support yet
        return;
    }

    uint8_t bytes_per_pixel = (bits_per_pixel + 7) >> 3;
    uint32_t min = std::min(width, height);
    uint32_t k = std::bit_width(min) - 1;

    for (uint32_t i = 0; i < width * static_cast<uint32_t>(height); i++) {
        uint32_t x = decode_morton2_x(i) & (min - 1);
        uint32_t y = decode_morton2_y(i) & (min - 1);
        uint32_t upper_bits = (i >> (2 * k)) << k;
        if (width >= height) {
            x |= upper_bits;
        } else {
            y |= upper_bits;
        }

        memcpy(dest + (y * width + x) * bytes_per_pixel, src + i * bytes_per_pixel, bytes_per_pixel);
    }
}

void tiled_texture_to_linear_texture(uint8_t *dest, const uint8_t *src, uint16_t width, uint16_t height, uint8_t bits_per_pixel) {
    // 32x32 block is assembled to tiled.
    if (bits_per_pixel % 8 != 0) {
        // Don't support yet
        return;
    }

    const uint32_t bpp = bits_per_pixel >> 3;
    const uint32_t width_in_tiles = (width + 31) >> 5;

    for (uint16_t y = 0; y < height; y++) {
        for (uint16_t x = 0; x < width; x++) {
            // Calculate texel address in tile
            const uint32_t texel_offset_in_tile = (x & 0b11111) | ((y & 0b11111) << 5);
            const uint32_t tile_address = (x >> 5) + width_in_tiles * (y >> 5);

            const uint32_t offset = ((tile_address << 10) | (texel_offset_in_tile)) * bpp;

            // Make scanline
            memcpy(dest + ((y * width) + x) * bpp, src + offset, bpp);
        }
    }
}

uint32_t get_compressed_size(SceGxmTextureBaseFormat base_format, uint32_t width, uint32_t height) {
    switch (base_format) {
    case SCE_GXM_TEXTURE_BASE_FORMAT_UBC1:
    case SCE_GXM_TEXTURE_BASE_FORMAT_UBC4:
    case SCE_GXM_TEXTURE_BASE_FORMAT_SBC4:
        return ((width + 3) / 4) * ((height + 3) / 4) * 8;
    case SCE_GXM_TEXTURE_BASE_FORMAT_UBC2:
    case SCE_GXM_TEXTURE_BASE_FORMAT_UBC3:
    case SCE_GXM_TEXTURE_BASE_FORMAT_UBC5:
    case SCE_GXM_TEXTURE_BASE_FORMAT_SBC5:
    case SCE_GXM_TEXTURE_BASE_FORMAT_UBC6H:
    case SCE_GXM_TEXTURE_BASE_FORMAT_SBC6H:
    case SCE_GXM_TEXTURE_BASE_FORMAT_UBC7:
        return ((width + 3) / 4) * ((height + 3) / 4) * 16;

        // each ASTC block is always 128 bits (16 bytes)
#define ASTC_FMT(b_x, b_y)                              \
    case SCE_GXM_TEXTURE_BASE_FORMAT_ASTC##b_x##x##b_y: \
        return ((width + b_x - 1) / b_x) * ((height + b_y - 1) / b_y) * 16;

#include "astc_formats.inc"
#undef ASTC_FMT

    default:
        LOG_ERROR("Invalid block compressed texture format: {}", fmt::underlying(base_format));
        return 0;
    }
}

// =========================== COMPRESSION ============================
// Some texture has block compression, when uncompressed will have swizzled layout. Since on some backend, no
// option is provided to make the GPU driver not try to translate the layout to linear, we have to do uncompress
// and unswizzled on the CPU.

// This BC decompression code is based on code from AMD GPUOpen's Compressonator

/**
 * \brief Decompresses one block of a BC1 texture and stores the resulting pixels at the appropriate offset in 'image'.
 *
 * \param block_storage     pointer to the block to decompress.
 * \param image             pointer to image where the decompressed pixel data should be stored.
 **/
static void decompress_block_bc1(const uint8_t *block_storage, uint32_t *image) {
    std::uint16_t n0 = static_cast<std::uint16_t>((block_storage[1] << 8) | block_storage[0]);
    std::uint16_t n1 = static_cast<std::uint16_t>((block_storage[3] << 8) | block_storage[2]);

    block_storage += 4;

    std::uint8_t r0 = (n0 & 0xF800) >> 8;
    std::uint8_t g0 = (n0 & 0x07E0) >> 3;
    std::uint8_t b0 = (n0 & 0x001F) << 3;

    std::uint8_t r1 = (n1 & 0xF800) >> 8;
    std::uint8_t g1 = (n1 & 0x07E0) >> 3;
    std::uint8_t b1 = (n1 & 0x001F) << 3;

    r0 |= r0 >> 5;
    r1 |= r1 >> 5;
    g0 |= g0 >> 6;
    g1 |= g1 >> 6;
    b0 |= b0 >> 5;
    b1 |= b1 >> 5;

    std::uint32_t c0 = 0xFF000000 | (b0 << 16) | (g0 << 8) | r0;
    std::uint32_t c1 = 0xFF000000 | (b1 << 16) | (g1 << 8) | r1;

    if (n0 > n1) {
        std::uint8_t r2 = static_cast<uint8_t>((2 * r0 + r1 + 1) / 3);
        std::uint8_t r3 = static_cast<uint8_t>((2 * r1 + r0 + 1) / 3);
        std::uint8_t g2 = static_cast<uint8_t>((2 * g0 + g1 + 1) / 3);
        std::uint8_t g3 = static_cast<uint8_t>((2 * g1 + g0 + 1) / 3);
        std::uint8_t b2 = static_cast<uint8_t>((2 * b0 + b1 + 1) / 3);
        std::uint8_t b3 = static_cast<uint8_t>((2 * b1 + b0 + 1) / 3);

        std::uint32_t c2 = 0xFF000000 | (b2 << 16) | (g2 << 8) | r2;
        std::uint32_t c3 = 0xFF000000 | (b3 << 16) | (g3 << 8) | r3;

        for (int i = 0; i < 16; ++i) {
            int index = (block_storage[i / 4] >> (i % 4 * 2)) & 0x03;
            switch (index) {
            case 0:
                image[i] = c0;
                break;
            case 1:
                image[i] = c1;
                break;
            case 2:
                image[i] = c2;
                break;
            case 3:
                image[i] = c3;
                break;
            }
        }
    } else {
        // Transparent decode
        std::uint8_t r2 = static_cast<uint8_t>((r0 + r1) / 2);
        std::uint8_t g2 = static_cast<uint8_t>((g0 + g1) / 2);
        std::uint8_t b2 = static_cast<uint8_t>((b0 + b1) / 2);

        std::uint32_t c2 = 0xFF000000 | (b2 << 16) | (g2 << 8) | r2;

        for (int i = 0; i < 16; ++i) {
            int index = (block_storage[i / 4] >> (i % 4 * 2)) & 0x03;
            switch (index) {
            case 0:
                image[i] = c0;
                break;
            case 1:
                image[i] = c1;
                break;
            case 2:
                image[i] = c2;
                break;
            case 3:
                image[i] = 0x00000000;
                break;
            }
        }
    }
}

/**
 * \brief Decompresses one block of a alpha texture and stores the resulting pixels at the appropriate offset in 'image'.
 *
 * \param block_storage     pointer to the block to decompress.
 * \param image             pointer to image where the decompressed pixel data should be stored.
 * \param offset            offset to where data should be written.
 * \param stride            stride between bytes to where data should be written.
 **/
static void decompress_block_alpha(const uint8_t *block_storage, uint8_t *image, const uint32_t offset, const uint32_t stride) {
    uint8_t alpha[8];

    alpha[0] = block_storage[0];
    alpha[1] = block_storage[1];

    if (alpha[0] > alpha[1]) {
        // 8-alpha block:  derive the other six alphas.
        // Bit code 000 = alpha_0, 001 = alpha_1, others are interpolated.
        alpha[2] = static_cast<uint8_t>((6 * alpha[0] + 1 * alpha[1] + 3) / 7); // bit code 010
        alpha[3] = static_cast<uint8_t>((5 * alpha[0] + 2 * alpha[1] + 3) / 7); // bit code 011
        alpha[4] = static_cast<uint8_t>((4 * alpha[0] + 3 * alpha[1] + 3) / 7); // bit code 100
        alpha[5] = static_cast<uint8_t>((3 * alpha[0] + 4 * alpha[1] + 3) / 7); // bit code 101
        alpha[6] = static_cast<uint8_t>((2 * alpha[0] + 5 * alpha[1] + 3) / 7); // bit code 110
        alpha[7] = static_cast<uint8_t>((1 * alpha[0] + 6 * alpha[1] + 3) / 7); // bit code 111
    } else {
        // 6-alpha block.
        // Bit code 000 = alpha_0, 001 = alpha_1, others are interpolated.
        alpha[2] = static_cast<uint8_t>((4 * alpha[0] + 1 * alpha[1] + 2) / 5); // Bit code 010
        alpha[3] = static_cast<uint8_t>((3 * alpha[0] + 2 * alpha[1] + 2) / 5); // Bit code 011
        alpha[4] = static_cast<uint8_t>((2 * alpha[0] + 3 * alpha[1] + 2) / 5); // Bit code 100
        alpha[5] = static_cast<uint8_t>((1 * alpha[0] + 4 * alpha[1] + 2) / 5); // Bit code 101
        alpha[6] = 0; // Bit code 110
        alpha[7] = 255; // Bit code 111
    }

    image += offset;

    image[stride * 0] = alpha[block_storage[2] & 0x07];
    image[stride * 1] = alpha[(block_storage[2] >> 3) & 0x07];
    image[stride * 2] = alpha[((block_storage[3] << 2) & 0x04) | ((block_storage[2] >> 6) & 0x03)];
    image[stride * 3] = alpha[(block_storage[3] >> 1) & 0x07];
    image[stride * 4] = alpha[(block_storage[3] >> 4) & 0x07];
    image[stride * 5] = alpha[((block_storage[4] << 1) & 0x06) | ((block_storage[3] >> 7) & 0x01)];
    image[stride * 6] = alpha[(block_storage[4] >> 2) & 0x07];
    image[stride * 7] = alpha[(block_storage[4] >> 5) & 0x07];
    image[stride * 8] = alpha[block_storage[5] & 0x07];
    image[stride * 9] = alpha[(block_storage[5] >> 3) & 0x07];
    image[stride * 10] = alpha[((block_storage[6] << 2) & 0x04) | ((block_storage[5] >> 6) & 0x03)];
    image[stride * 11] = alpha[(block_storage[6] >> 1) & 0x07];
    image[stride * 12] = alpha[(block_storage[6] >> 4) & 0x07];
    image[stride * 13] = alpha[((block_storage[7] << 1) & 0x06) | ((block_storage[6] >> 7) & 0x01)];
    image[stride * 14] = alpha[(block_storage[7] >> 2) & 0x07];
    image[stride * 15] = alpha[(block_storage[7] >> 5) & 0x07];
}

/**
 * \brief Decompresses one block of a signed alpha texture and stores the resulting pixels at the appropriate offset in 'image'.
 *
 * \param block_storage     pointer to the block to decompress.
 * \param image             pointer to image where the decompressed pixel data should be stored.
 * \param offset            offset to where data should be written.
 * \param stride            stride between bytes to where data should be written.
 **/
static void decompress_block_alpha_signed(const uint8_t *block_storage, uint8_t *image, const uint32_t offset, const uint32_t stride) {
    int8_t alpha[8];

    alpha[0] = static_cast<int8_t>(block_storage[0]);
    alpha[1] = static_cast<int8_t>(block_storage[1]);

    if (alpha[0] > alpha[1]) {
        // 8-alpha block:  derive the other six alphas.
        // Bit code 000 = alpha_0, 001 = alpha_1, others are interpolated.
        alpha[2] = static_cast<int8_t>((6 * alpha[0] + 1 * alpha[1] + 3) / 7); // bit code 010
        alpha[3] = static_cast<int8_t>((5 * alpha[0] + 2 * alpha[1] + 3) / 7); // bit code 011
        alpha[4] = static_cast<int8_t>((4 * alpha[0] + 3 * alpha[1] + 3) / 7); // bit code 100
        alpha[5] = static_cast<int8_t>((3 * alpha[0] + 4 * alpha[1] + 3) / 7); // bit code 101
        alpha[6] = static_cast<int8_t>((2 * alpha[0] + 5 * alpha[1] + 3) / 7); // bit code 110
        alpha[7] = static_cast<int8_t>((1 * alpha[0] + 6 * alpha[1] + 3) / 7); // bit code 111
    } else {
        // 6-alpha block.
        // Bit code 000 = alpha_0, 001 = alpha_1, others are interpolated.
        alpha[2] = static_cast<int8_t>((4 * alpha[0] + 1 * alpha[1] + 2) / 5); // Bit code 010
        alpha[3] = static_cast<int8_t>((3 * alpha[0] + 2 * alpha[1] + 2) / 5); // Bit code 011
        alpha[4] = static_cast<int8_t>((2 * alpha[0] + 3 * alpha[1] + 2) / 5); // Bit code 100
        alpha[5] = static_cast<int8_t>((1 * alpha[0] + 4 * alpha[1] + 2) / 5); // Bit code 101
        alpha[6] = -128; // Bit code 110
        alpha[7] = 127; // Bit code 111
    }

    image += offset;

    image[stride * 0] = static_cast<uint8_t>(alpha[block_storage[2] & 0x07]);
    image[stride * 1] = static_cast<uint8_t>(alpha[(block_storage[2] >> 3) & 0x07]);
    image[stride * 2] = static_cast<uint8_t>(alpha[((block_storage[3] << 2) & 0x04) | ((block_storage[2] >> 6) & 0x03)]);
    image[stride * 3] = static_cast<uint8_t>(alpha[(block_storage[3] >> 1) & 0x07]);
    image[stride * 4] = static_cast<uint8_t>(alpha[(block_storage[3] >> 4) & 0x07]);
    image[stride * 5] = static_cast<uint8_t>(alpha[((block_storage[4] << 1) & 0x06) | ((block_storage[3] >> 7) & 0x01)]);
    image[stride * 6] = static_cast<uint8_t>(alpha[(block_storage[4] >> 2) & 0x07]);
    image[stride * 7] = static_cast<uint8_t>(alpha[(block_storage[4] >> 5) & 0x07]);
    image[stride * 8] = static_cast<uint8_t>(alpha[block_storage[5] & 0x07]);
    image[stride * 9] = static_cast<uint8_t>(alpha[(block_storage[5] >> 3) & 0x07]);
    image[stride * 10] = static_cast<uint8_t>(alpha[((block_storage[6] << 2) & 0x04) | ((block_storage[5] >> 6) & 0x03)]);
    image[stride * 11] = static_cast<uint8_t>(alpha[(block_storage[6] >> 1) & 0x07]);
    image[stride * 12] = static_cast<uint8_t>(alpha[(block_storage[6] >> 4) & 0x07]);
    image[stride * 13] = static_cast<uint8_t>(alpha[((block_storage[7] << 1) & 0x06) | ((block_storage[6] >> 7) & 0x01)]);
    image[stride * 14] = static_cast<uint8_t>(alpha[(block_storage[7] >> 2) & 0x07]);
    image[stride * 15] = static_cast<uint8_t>(alpha[(block_storage[7] >> 5) & 0x07]);
}

/**
 * \brief Decompresses one block of a BC2 texture and stores the resulting pixels at the appropriate offset in 'image'.
 *
 * \param block_storage     pointer to the block to decompress.
 * \param image             pointer to image where the decompressed pixel data should be stored.
 **/
static void decompress_block_bc2(const uint8_t *block_storage, uint32_t *image) {
    decompress_block_bc1(block_storage + 8, image);

    for (int i = 0; i < 8; i++) {
        image[2 * i] = (((block_storage[i] & 0x0F) | ((block_storage[i] & 0x0F) << 4)) << 24) | (image[2 * i] & 0x00FFFFFF);
        image[2 * i + 1] = (((block_storage[i] & 0xF0) | ((block_storage[i] & 0xF0) >> 4)) << 24) | (image[2 * i + 1] & 0x00FFFFFF);
    }
}

/**
 * \brief Decompresses one block of a BC3 texture and stores the resulting pixels at the appropriate offset in 'image'.
 *
 * \param block_storage     pointer to the block to decompress.
 * \param image             pointer to image where the decompressed pixel data should be stored.
 **/
static void decompress_block_bc3(const uint8_t *block_storage, uint32_t *image) {
    decompress_block_bc1(block_storage + 8, image);
    decompress_block_alpha(block_storage, reinterpret_cast<std::uint8_t *>(image), 3, 4);
}

/**
 * \brief Decompresses one block of a BC4U texture and stores the resulting pixels at the appropriate offset in 'image'.
 *
 * \param block_storage     pointer to the block to decompress.
 * \param image             pointer to image where the decompressed pixel data should be stored.
 **/
static void decompress_block_bc4u(const uint8_t *block_storage, uint8_t *image) {
    for (int i = 0; i < 16; i++)
        image[i] = 0x00;
    decompress_block_alpha(block_storage, image, 0, 1);
}

/**
 * \brief Decompresses one block of a BC4S texture and stores the resulting pixels at the appropriate offset in 'image'.
 *
 * \param block_storage     pointer to the block to decompress.
 * \param image             pointer to image where the decompressed pixel data should be stored.
 **/
static void decompress_block_bc4s(const uint8_t *block_storage, uint8_t *image) {
    for (int i = 0; i < 16; i++)
        image[i] = 0x00;
    decompress_block_alpha_signed(block_storage, image, 0, 1);
}

/**
 * \brief Decompresses one block of a BC5U texture and stores the resulting pixels at the appropriate offset in 'image'.
 *
 * \param block_storage     pointer to the block to decompress.
 * \param image             pointer to image where the decompressed pixel data should be stored.
 **/
static void decompress_block_bc5u(const uint8_t *block_storage, uint16_t *image) {
    for (int i = 0; i < 16; i++)
        image[i] = 0x0000;
    decompress_block_alpha(block_storage, reinterpret_cast<uint8_t *>(image), 0, 2);
    decompress_block_alpha(block_storage + 8, reinterpret_cast<uint8_t *>(image), 1, 2);
}

/**
 * \brief Decompresses one block of a BC5S texture and stores the resulting pixels at the appropriate offset in 'image'.
 *
 * \param block_storage     pointer to the block to decompress.
 * \param image             pointer to image where the decompressed pixel data should be stored.
 **/
static void decompress_block_bc5s(const uint8_t *block_storage, uint16_t *image) {
    for (int i = 0; i < 16; i++)
        image[i] = 0x0000;
    decompress_block_alpha_signed(block_storage, reinterpret_cast<uint8_t *>(image), 0, 2);
    decompress_block_alpha_signed(block_storage + 8, reinterpret_cast<uint8_t *>(image), 1, 2);
}

void decompress_bc_image(uint32_t width, uint32_t height, const uint8_t *block_storage, uint32_t *image, const uint8_t format_id) {
    const uint32_t block_count_x = (width + 3) / 4;
    const uint32_t block_count_y = (height + 3) / 4;
    const uint32_t block_size = (format_id != 1 && format_id != 4 && format_id != 5) ? 16 : 8;
    const uint32_t line_size = block_count_x * 4;

    auto decompress_bcn = [=, &block_storage]<typename T, typename F>(T _, F decompress_func) {
        T temp_block_result[16] = {};
        T *img = reinterpret_cast<T *>(image);

        for (uint32_t j = 0; j < block_count_y; j++) {
            for (uint32_t i = 0; i < block_count_x; i++) {
                decompress_func(block_storage, temp_block_result);

                const uint32_t offset = j * 4 * line_size + i * 4;
                for (uint32_t delta = 0; delta < 16; delta++) {
                    img[offset + (delta % 4) + ((delta / 4) * line_size)] = temp_block_result[delta];
                }

                block_storage += block_size;
            }
        }
    };

    switch (format_id) {
    case 1:
        decompress_bcn(uint32_t(), decompress_block_bc1);
        break;

    case 2:
        decompress_bcn(uint32_t(), decompress_block_bc2);
        break;

    case 3:
        decompress_bcn(uint32_t(), decompress_block_bc3);
        break;

    case 4:
        decompress_bcn(uint8_t(), decompress_block_bc4u);
        break;

    case 5:
        decompress_bcn(uint8_t(), decompress_block_bc4s);
        break;

    case 6:
        decompress_bcn(uint16_t(), decompress_block_bc5u);
        break;

    case 7:
        decompress_bcn(uint16_t(), decompress_block_bc5s);
        break;
    }
}

/**
 * \brief Solves Z-order on all the blocks of a block compressed texture and stores the resulting pixels in 'dest'.
 *
 * Output results is in format RGBA, with each channel being 8 bits.
 *
 * \param width     Texture width.
 * \param height    Texture height.
 * \param src       Pointer to compressed blocks.
 * \param dest      Pointer to the image where the decompressed pixels will be stored.
 */
void resolve_z_order_compressed_image(uint32_t width, uint32_t height, const uint8_t *src, uint8_t *dest, const uint32_t block_size) {
    uint32_t block_count_x = (width + 3) / 4;
    uint32_t block_count_y = (height + 3) / 4;

    uint32_t min = std::min(block_count_x, block_count_y);
    uint32_t k = std::bit_width(min) - 1;

    uint32_t block_count = block_count_x * block_count_y;
    for (uint32_t i = 0; i < block_count; i++) {
        uint32_t x = decode_morton2_x(i) & (min - 1);
        uint32_t y = decode_morton2_y(i) & (min - 1);
        uint32_t upper_bits = (i >> (2 * k)) << k;
        if (block_count_x >= block_count_y) {
            x |= upper_bits;
        } else {
            y |= upper_bits;
        }

        std::memcpy(dest + (y * block_count_x + x) * block_size, src + i * block_size, block_size);
    }
}

} // namespace renderer::texture
