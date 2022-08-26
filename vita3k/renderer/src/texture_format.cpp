// Vita3K emulator project
// Copyright (C) 2021 Vita3K team
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

#include <gxm/functions.h>
#include <gxm/types.h>
#include <shader/spirv_recompiler.h>
#include <util/log.h>

namespace renderer::texture {

float get_integral_query_format(const SceGxmTextureBaseFormat format) {
    if ((format == SCE_GXM_TEXTURE_BASE_FORMAT_S8) || (format == SCE_GXM_TEXTURE_BASE_FORMAT_S8S8) || (format == SCE_GXM_TEXTURE_BASE_FORMAT_S8S8S8) || (format == SCE_GXM_TEXTURE_BASE_FORMAT_S8S8S8S8)) {
        return shader::INTEGRAL_TEX_QUERY_TYPE_8BIT_SIGNED;
    }

    if ((format == SCE_GXM_TEXTURE_BASE_FORMAT_U16) || (format == SCE_GXM_TEXTURE_BASE_FORMAT_U16U16) || (format == SCE_GXM_TEXTURE_BASE_FORMAT_U16U16U16U16) || (format == SCE_GXM_TEXTURE_BASE_FORMAT_S16) || (format == SCE_GXM_TEXTURE_BASE_FORMAT_S16S16) || (format == SCE_GXM_TEXTURE_BASE_FORMAT_S16S16S16S16)) {
        return shader::INTEGRAL_TEX_QUERY_TYPE_16BIT;
    }

    if ((format == SCE_GXM_TEXTURE_BASE_FORMAT_U32) || (format == SCE_GXM_TEXTURE_BASE_FORMAT_U32U32) || (format == SCE_GXM_TEXTURE_BASE_FORMAT_S32)) {
        return shader::INTEGRAL_TEX_QUERY_TYPE_32BIT;
    }

    return shader::INTEGRAL_TEX_QUERY_TYPE_8BIT_UNSIGNED;
}

size_t bits_per_pixel(SceGxmTextureBaseFormat base_format) {
    switch (base_format) {
    case SCE_GXM_TEXTURE_BASE_FORMAT_U8:
    case SCE_GXM_TEXTURE_BASE_FORMAT_S8:
        return 8;
    case SCE_GXM_TEXTURE_BASE_FORMAT_U4U4U4U4:
    case SCE_GXM_TEXTURE_BASE_FORMAT_U8U3U3U2:
    case SCE_GXM_TEXTURE_BASE_FORMAT_U1U5U5U5:
    case SCE_GXM_TEXTURE_BASE_FORMAT_U5U6U5:
    case SCE_GXM_TEXTURE_BASE_FORMAT_S5S5U6:
    case SCE_GXM_TEXTURE_BASE_FORMAT_U8U8:
    case SCE_GXM_TEXTURE_BASE_FORMAT_S8S8:
    case SCE_GXM_TEXTURE_BASE_FORMAT_U16:
    case SCE_GXM_TEXTURE_BASE_FORMAT_S16:
    case SCE_GXM_TEXTURE_BASE_FORMAT_F16:
        return 16;
    case SCE_GXM_TEXTURE_BASE_FORMAT_U8U8U8U8:
    case SCE_GXM_TEXTURE_BASE_FORMAT_S8S8S8S8:
    case SCE_GXM_TEXTURE_BASE_FORMAT_U2U10U10U10:
    case SCE_GXM_TEXTURE_BASE_FORMAT_U16U16:
    case SCE_GXM_TEXTURE_BASE_FORMAT_S16S16:
    case SCE_GXM_TEXTURE_BASE_FORMAT_F16F16:
    case SCE_GXM_TEXTURE_BASE_FORMAT_F32:
    case SCE_GXM_TEXTURE_BASE_FORMAT_F32M:
    case SCE_GXM_TEXTURE_BASE_FORMAT_X8S8S8U8:
    case SCE_GXM_TEXTURE_BASE_FORMAT_X8U24:
    case SCE_GXM_TEXTURE_BASE_FORMAT_U32:
    case SCE_GXM_TEXTURE_BASE_FORMAT_S32:
    case SCE_GXM_TEXTURE_BASE_FORMAT_SE5M9M9M9:
    case SCE_GXM_TEXTURE_BASE_FORMAT_F11F11F10:
        return 32;
    case SCE_GXM_TEXTURE_BASE_FORMAT_F16F16F16F16:
    case SCE_GXM_TEXTURE_BASE_FORMAT_U16U16U16U16:
    case SCE_GXM_TEXTURE_BASE_FORMAT_S16S16S16S16:
    case SCE_GXM_TEXTURE_BASE_FORMAT_F32F32:
    case SCE_GXM_TEXTURE_BASE_FORMAT_U32U32:
        return 64;
    case SCE_GXM_TEXTURE_BASE_FORMAT_PVRT2BPP:
        return 2;
    case SCE_GXM_TEXTURE_BASE_FORMAT_PVRT4BPP:
        return 4;
    case SCE_GXM_TEXTURE_BASE_FORMAT_PVRTII2BPP:
        return 2;
    case SCE_GXM_TEXTURE_BASE_FORMAT_PVRTII4BPP:
        return 4;
    case SCE_GXM_TEXTURE_BASE_FORMAT_UBC1:
        return 4;
    case SCE_GXM_TEXTURE_BASE_FORMAT_UBC2:
    case SCE_GXM_TEXTURE_BASE_FORMAT_UBC3:
        return 8;
    case SCE_GXM_TEXTURE_BASE_FORMAT_UBC4:
    case SCE_GXM_TEXTURE_BASE_FORMAT_SBC4:
        return 4;
    case SCE_GXM_TEXTURE_BASE_FORMAT_UBC5:
    case SCE_GXM_TEXTURE_BASE_FORMAT_SBC5:
        return 8;
    case SCE_GXM_TEXTURE_BASE_FORMAT_YUV420P2:
    case SCE_GXM_TEXTURE_BASE_FORMAT_YUV420P3:
        return 12;
    case SCE_GXM_TEXTURE_BASE_FORMAT_YUV422:
        return 16;
    case SCE_GXM_TEXTURE_BASE_FORMAT_P4:
        return 4;
    case SCE_GXM_TEXTURE_BASE_FORMAT_P8:
        return 8;
    case SCE_GXM_TEXTURE_BASE_FORMAT_U8U8U8:
    case SCE_GXM_TEXTURE_BASE_FORMAT_S8S8S8:
        return 24;
    case SCE_GXM_TEXTURE_BASE_FORMAT_U2F10F10F10:
        return 32;
    }

    return 0;
}

size_t texture_size(const SceGxmTexture &texture) {
    const SceGxmTextureFormat format = gxm::get_format(&texture);
    const SceGxmTextureBaseFormat base_format = gxm::get_base_format(format);
    const size_t width = gxm::get_width(&texture);
    const size_t height = gxm::get_height(&texture);
    if ((base_format == SCE_GXM_TEXTURE_BASE_FORMAT_PVRT2BPP) || (base_format == SCE_GXM_TEXTURE_BASE_FORMAT_PVRTII2BPP)) {
        return ((width + 7) / 8 * 8) * ((height + 3) / 4 * 4) / 4;
    } else if ((base_format == SCE_GXM_TEXTURE_BASE_FORMAT_PVRT4BPP) || (base_format == SCE_GXM_TEXTURE_BASE_FORMAT_PVRTII4BPP)) {
        return ((width + 3) / 4 * 4) * ((height + 3) / 4 * 4) / 2;
    }
    const size_t stride = (width + 7) & ~7; // NOTE: This is correct only with linear textures.
    const size_t bpp = bits_per_pixel(base_format);
    const size_t size = (bpp * stride * height) / 8;
    return size;
}

bool convert_base_texture_format_to_base_color_format(SceGxmTextureBaseFormat format, SceGxmColorBaseFormat &color_format) {
    static const std::map<std::uint32_t, std::uint32_t> TEXTURE_TO_COLOR_FORMAT_MAPPING = {
        { SCE_GXM_TEXTURE_BASE_FORMAT_U8U8U8U8, SCE_GXM_COLOR_BASE_FORMAT_U8U8U8U8 },
        // { SCE_GXM_TEXTURE_BASE_FORMAT_U8U8U8, SCE_GXM_COLOR_BASE_FORMAT_U8U8U8 },
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

// Based on this: http://xen.firefly.nu/up/rearrange.c.html
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

static uint32_t decode_morton2_x(uint32_t code) {
    return compact_one_by_one(code >> 0);
}

static uint32_t decode_morton2_y(uint32_t code) {
    return compact_one_by_one(code >> 1);
}

void swizzled_texture_to_linear_texture(uint8_t *dest, const uint8_t *src, uint16_t width, uint16_t height, uint8_t bits_per_pixel) {
    if (bits_per_pixel % 8 != 0) {
        // Don't support yet
        return;
    }

    uint8_t bytes_per_pixel = (bits_per_pixel + 7) >> 3;

    for (uint32_t i = 0; i < static_cast<uint32_t>(width * height); i++) {
        size_t min = width < height ? width : height;
        size_t k = static_cast<size_t>(log2(min));

        size_t x, y;
        if (height < width) {
            // XXXyxyxyx → XXXxxxyyy
            size_t j = i >> (2 * k) << (2 * k)
                | (decode_morton2_y(i) & (min - 1)) << k
                | (decode_morton2_x(i) & (min - 1)) << 0;
            x = j / height;
            y = j % height;
        } else {
            // YYYyxyxyx → YYYyyyxxx
            size_t j = i >> (2 * k) << (2 * k)
                | (decode_morton2_x(i) & (min - 1)) << k
                | (decode_morton2_y(i) & (min - 1)) << 0;
            x = j % width;
            y = j / width;
        }

        if (y >= height || x >= width)
            continue;

        std::memcpy(dest + (y * width + x) * bytes_per_pixel, src + i * bytes_per_pixel, bytes_per_pixel);
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

bool is_compressed_format(SceGxmTextureBaseFormat base_format) {
    switch (base_format) {
    case SCE_GXM_TEXTURE_BASE_FORMAT_UBC1:
    case SCE_GXM_TEXTURE_BASE_FORMAT_UBC2:
    case SCE_GXM_TEXTURE_BASE_FORMAT_UBC3:
    case SCE_GXM_TEXTURE_BASE_FORMAT_UBC4:
    case SCE_GXM_TEXTURE_BASE_FORMAT_SBC4:
    case SCE_GXM_TEXTURE_BASE_FORMAT_UBC5:
    case SCE_GXM_TEXTURE_BASE_FORMAT_SBC5:
        return true;
    default:
        break;
    }

    return false;
}

size_t get_compressed_size(SceGxmTextureBaseFormat base_format, std::uint32_t width, std::uint32_t height) {
    switch (base_format) {
    case SCE_GXM_TEXTURE_BASE_FORMAT_UBC1:
    case SCE_GXM_TEXTURE_BASE_FORMAT_UBC4:
    case SCE_GXM_TEXTURE_BASE_FORMAT_SBC4:
        return ((width + 3) / 4) * ((height + 3) / 4) * 8;
    case SCE_GXM_TEXTURE_BASE_FORMAT_UBC2:
    case SCE_GXM_TEXTURE_BASE_FORMAT_UBC3:
    case SCE_GXM_TEXTURE_BASE_FORMAT_UBC5:
    case SCE_GXM_TEXTURE_BASE_FORMAT_SBC5:
        return ((width + 3) / 4) * ((height + 3) / 4) * 16;
    default:
        LOG_ERROR("Invalid block compressed texture format: {}", base_format);
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
static void decompress_block_bc1(const std::uint8_t *block_storage, std::uint32_t *image) {
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
static void decompress_block_alpha(const std::uint8_t *block_storage, std::uint8_t *image, const std::uint32_t offset, const std::uint32_t stride) {
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
static void decompress_block_alpha_signed(const std::uint8_t *block_storage, std::uint8_t *image, const std::uint32_t offset, const std::uint32_t stride) {
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
static void decompress_block_bc2(const std::uint8_t *block_storage, std::uint32_t *image) {
    decompress_block_bc1(block_storage + 8, image);

    for (int i = 0; i < 16; i += 2) {
        image[i] = (((block_storage[i] & 0x0F) | ((block_storage[i] & 0x0F) << 4)) << 24) | (image[i] & 0x00FFFFFF);
    }

    for (int i = 1; i < 16; i += 2) {
        image[i] = (((block_storage[i] & 0xF0) | ((block_storage[i] & 0xF0) >> 4)) << 24) | (image[i] & 0x00FFFFFF);
    }
}

/**
 * \brief Decompresses one block of a BC3 texture and stores the resulting pixels at the appropriate offset in 'image'.
 *
 * \param block_storage     pointer to the block to decompress.
 * \param image             pointer to image where the decompressed pixel data should be stored.
 **/
static void decompress_block_bc3(const std::uint8_t *block_storage, std::uint32_t *image) {
    decompress_block_bc1(block_storage + 8, image);
    decompress_block_alpha(block_storage, reinterpret_cast<std::uint8_t *>(image), 3, 4);
}

/**
 * \brief Decompresses one block of a BC4U texture and stores the resulting pixels at the appropriate offset in 'image'.
 *
 * \param block_storage     pointer to the block to decompress.
 * \param image             pointer to image where the decompressed pixel data should be stored.
 **/
static void decompress_block_bc4u(const std::uint8_t *block_storage, std::uint32_t *image) {
    for (int i = 0; i < 16; i++)
        image[i] = 0x00000000;
    decompress_block_alpha(block_storage, reinterpret_cast<std::uint8_t *>(image), 0, 4);
}

/**
 * \brief Decompresses one block of a BC4S texture and stores the resulting pixels at the appropriate offset in 'image'.
 *
 * \param block_storage     pointer to the block to decompress.
 * \param image             pointer to image where the decompressed pixel data should be stored.
 **/
static void decompress_block_bc4s(const std::uint8_t *block_storage, std::uint32_t *image) {
    for (int i = 0; i < 16; i++)
        image[i] = 0x00000000;
    decompress_block_alpha_signed(block_storage, reinterpret_cast<std::uint8_t *>(image), 0, 4);
}

/**
 * \brief Decompresses one block of a BC5U texture and stores the resulting pixels at the appropriate offset in 'image'.
 *
 * \param block_storage     pointer to the block to decompress.
 * \param image             pointer to image where the decompressed pixel data should be stored.
 **/
static void decompress_block_bc5u(const std::uint8_t *block_storage, std::uint32_t *image) {
    for (int i = 0; i < 16; i++)
        image[i] = 0x00000000;
    decompress_block_alpha(block_storage, reinterpret_cast<std::uint8_t *>(image), 0, 4);
    decompress_block_alpha(block_storage + 8, reinterpret_cast<std::uint8_t *>(image), 1, 4);
}

/**
 * \brief Decompresses one block of a BC5S texture and stores the resulting pixels at the appropriate offset in 'image'.
 *
 * \param block_storage     pointer to the block to decompress.
 * \param image             pointer to image where the decompressed pixel data should be stored.
 **/
static void decompress_block_bc5s(const std::uint8_t *block_storage, std::uint32_t *image) {
    for (int i = 0; i < 16; i++)
        image[i] = 0x00000000;
    decompress_block_alpha_signed(block_storage, reinterpret_cast<std::uint8_t *>(image), 0, 4);
    decompress_block_alpha_signed(block_storage + 8, reinterpret_cast<std::uint8_t *>(image), 1, 4);
}

/**
 * \brief Decompresses all the blocks of a block compressed texture and stores the resulting pixels in 'image'.
 *
 * Output results is in format RGBA, with each channel being 8 bits.
 *
 * \param width             Texture width.
 * \param height            Texture height.
 * \param block_storage     Pointer to compressed blocks.
 * \param image             Pointer to the image where the decompressed pixels will be stored.
 * \param bc_type           Block compressed type. BC1 (DXT1), BC2 (DXT3), BC3 (DXT5), BC4U (RGTC1), BC4S (RGTC1), BC5U (RGTC2) or BC5S (RGTC2).
 */
void decompress_bc_swizz_image(std::uint32_t width, std::uint32_t height, const std::uint8_t *block_storage, std::uint32_t *image, const std::uint8_t bc_type) {
    std::uint32_t block_count_x = (width + 3) / 4;
    std::uint32_t block_count_y = (height + 3) / 4;
    std::size_t block_size = (bc_type != 1 && bc_type != 4 && bc_type != 5) ? 16 : 8;

    std::uint32_t temp_block_result[16] = {};

    // Z-order curve inverse table
    static const int z_order_curve_inv[] = {
        0, 2, 8, 10,
        1, 3, 9, 11,
        4, 6, 12, 14,
        5, 7, 13, 15
    };

    for (std::uint32_t j = 0; j < block_count_y; j++) {
        for (std::uint32_t i = 0; i < block_count_x; i++) {
            switch (bc_type) {
            case 1:
                decompress_block_bc1(block_storage, temp_block_result);
                break;

            case 2:
                decompress_block_bc2(block_storage, temp_block_result);
                break;

            case 3:
                decompress_block_bc3(block_storage, temp_block_result);
                break;

            case 4:
                decompress_block_bc4u(block_storage, temp_block_result);
                break;

            case 5:
                decompress_block_bc4s(block_storage, temp_block_result);
                break;

            case 6:
                decompress_block_bc5u(block_storage, temp_block_result);
                break;

            case 7:
                decompress_block_bc5s(block_storage, temp_block_result);
                break;
            }

            for (int b = 0; b < 16; b++) {
                image[z_order_curve_inv[b]] = temp_block_result[b];
            }

            block_storage += block_size;
            image += 16;
        }
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
 * \param bc_type   Block compressed type. BC1 (DXT1), BC2 (DXT3), BC3 (DXT5), BC4U (RGTC1), BC4S (RGTC1), BC5U (RGTC2 or BC5S (RGTC2).
 */
void resolve_z_order_compressed_image(std::uint32_t width, std::uint32_t height, const std::uint8_t *src, std::uint8_t *dest, const std::uint8_t bc_type) {
    std::uint32_t block_count_x = (width + 3) / 4;
    std::uint32_t block_count_y = (height + 3) / 4;
    std::size_t block_size = (bc_type != 1 && bc_type != 4 && bc_type != 5) ? 16 : 8;

    std::uint32_t temp_block_result[16] = {};

    size_t min = block_count_x < block_count_y ? block_count_x : block_count_y;
    size_t k = static_cast<size_t>(log2(min));

    bool x_first = block_count_y < block_count_x;

    uint32_t block_count = static_cast<uint32_t>(block_count_x * block_count_y);
    for (uint32_t i = 0; i < block_count; i++) {
        size_t x, y;
        if (x_first) {
            // XXXyxyxyx → XXXxxxyyy
            size_t j = i >> (2 * k) << (2 * k)
                | (decode_morton2_y(i) & (min - 1)) << k
                | (decode_morton2_x(i) & (min - 1)) << 0;
            x = j / block_count_y;
            y = j % block_count_y;
        } else {
            // YYYyxyxyx → YYYyyyxxx
            size_t j = i >> (2 * k) << (2 * k)
                | (decode_morton2_x(i) & (min - 1)) << k
                | (decode_morton2_y(i) & (min - 1)) << 0;
            x = j % block_count_x;
            y = j / block_count_x;
        }

        if (y >= block_count_y || x >= block_count_x)
            continue;

        std::memcpy(dest + (y * block_count_x + x) * block_size, src + i * block_size, block_size);
    }
}

} // namespace renderer::texture
