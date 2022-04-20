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

namespace renderer::texture {

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

bool is_compressed_format(SceGxmTextureBaseFormat base_format, std::uint32_t width, std::uint32_t height, size_t &source_size) {
    switch (base_format) {
    case SCE_GXM_TEXTURE_BASE_FORMAT_UBC1:
    case SCE_GXM_TEXTURE_BASE_FORMAT_UBC2:
    case SCE_GXM_TEXTURE_BASE_FORMAT_UBC3:
        return ((width + 3) / 4) * ((height + 3) / 4) * ((base_format == SCE_GXM_TEXTURE_BASE_FORMAT_UBC1) ? 8 : 16);
    default:
        break;
    }

    return 0;
}

// =========================== COMPRESSION ============================
// Some texture has block compression, when uncompressed will have swizzled layout. Since on some backend, no
// option is provided to make the GPU driver not try to translate the layout to linear, we have to do uncompress
// and unswizzled on the CPU.

// This is a modified version of Benjamin Dobell's DXT decompression for compatible with our codebase and future OS.

/**
 * \brief Helper method that packs RGBA channels into a single 4 byte pixel, but reversed for little endian.
 *
 * \param r   red channel.
 * \param g   green channel.
 * \param b   blue channel.
 * \param a   alpha channel.
 */
static std::uint32_t pack_rgba_reversed(std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a) {
    return ((a << 24) | (b << 16) | (g << 8) | r);
}

/**
 * \brief Decompresses one block of a DXT1 texture and stores the resulting pixels at the appropriate offset in 'image',
 *        with custom alpha table.
 *
 * \param x                 x-coordinate of the first pixel in the block.
 * \param y                 y-coordinate of the first pixel in the block.
 * \param width             width of the texture being decompressed.
 * \param height            height of the texture being decompressed.
 * \param block_storage     pointer to the block to decompress.
 * \param image             pointer to image where the decompressed pixel data should be stored.
 * \param alpha_table       alpha lookup table.
 **/
static void decompress_block_dxt1_shared(std::uint32_t x, std::uint32_t y, std::uint32_t width, const std::uint8_t *block_storage, std::uint32_t *image,
    const std::uint8_t *alpha_table, const bool transparent_on_black) {
    std::uint16_t color0 = *reinterpret_cast<const std::uint16_t *>(block_storage);
    std::uint16_t color1 = *reinterpret_cast<const std::uint16_t *>(block_storage + 2);

    std::uint32_t temp;

    temp = (color0 >> 11) * 255 + 16;
    std::uint8_t r0 = (std::uint8_t)((temp / 32 + temp) / 32);
    temp = ((color0 & 0x07E0) >> 5) * 255 + 32;
    std::uint8_t g0 = (std::uint8_t)((temp / 64 + temp) / 64);
    temp = (color0 & 0x001F) * 255 + 16;
    std::uint8_t b0 = (std::uint8_t)((temp / 32 + temp) / 32);

    temp = (color1 >> 11) * 255 + 16;
    std::uint8_t r1 = (std::uint8_t)((temp / 32 + temp) / 32);
    temp = ((color1 & 0x07E0) >> 5) * 255 + 32;
    std::uint8_t g1 = (std::uint8_t)((temp / 64 + temp) / 64);
    temp = (color1 & 0x001F) * 255 + 16;
    std::uint8_t b1 = (std::uint8_t)((temp / 32 + temp) / 32);

    std::uint32_t code = *reinterpret_cast<const std::uint32_t *>(block_storage + 4);

    for (int j = 0; j < 4; j++) {
        for (int i = 0; i < 4; i++) {
            std::uint32_t final_color = 0;
            std::uint8_t positionCode = (code >> 2 * (4 * j + i)) & 0x03;
            std::uint8_t alpha = alpha_table[j * 4 + i];

            if (color0 > color1) {
                switch (positionCode) {
                case 0:
                    final_color = pack_rgba_reversed(r0, g0, b0, alpha);
                    break;
                case 1:
                    final_color = pack_rgba_reversed(r1, g1, b1, alpha);
                    break;
                case 2:
                    final_color = pack_rgba_reversed((2 * r0 + r1) / 3, (2 * g0 + g1) / 3, (2 * b0 + b1) / 3, alpha);
                    break;
                case 3:
                    final_color = pack_rgba_reversed((r0 + 2 * r1) / 3, (g0 + 2 * g1) / 3, (b0 + 2 * b1) / 3, alpha);
                    break;
                default:
                    final_color = 0xFFFFFFFF;
                    break;
                }
            } else {
                switch (positionCode) {
                case 0:
                    final_color = pack_rgba_reversed(r0, g0, b0, alpha);
                    break;
                case 1:
                    final_color = pack_rgba_reversed(r1, g1, b1, alpha);
                    break;
                case 2:
                    final_color = pack_rgba_reversed((r0 + r1) / 2, (g0 + g1) / 2, (b0 + b1) / 2, alpha);
                    break;
                case 3:
                    final_color = pack_rgba_reversed(0, 0, 0, transparent_on_black ? 0 : alpha);
                    break;
                default:
                    final_color = 0xFFFFFFFF;
                    break;
                }
            }

            image[j * 4 + i] = final_color;
        }
    }
}

static void decompress_block_dxt1(std::uint32_t x, std::uint32_t y, std::uint32_t width, const std::uint8_t *block_storage, std::uint32_t *image) {
    static constexpr std::uint8_t alpha_table[] = {
        255, 255, 255, 255,
        255, 255, 255, 255,
        255, 255, 255, 255,
        255, 255, 255, 255
    };

    decompress_block_dxt1_shared(x, y, width, block_storage, image, alpha_table, true);
}

/**
 * \brief Decompresses one block of a DXT3 texture and stores the resulting pixels at the appropriate offset in 'image'.
 *
 * \param x                 x-coordinate of the first pixel in the block.
 * \param y                 y-coordinate of the first pixel in the block.
 * \param width             width of the texture being decompressed.
 * \param height            height of the texture being decompressed.
 * \param block_storage     pointer to the block to decompress.
 * \param image             pointer to image where the decompressed pixel data should be stored.
 **/
static void decompress_block_dxt3(std::uint32_t x, std::uint32_t y, std::uint32_t width, const std::uint8_t *block_storage, std::uint32_t *image) {
    std::uint8_t alpha_table[16] = { 0 };

    for (int i = 0; i < 4; ++i) {
        const std::uint16_t *alpha_data = reinterpret_cast<const std::uint16_t *>(block_storage);

        alpha_table[i * 4 + 0] = (((*alpha_data) >> 0) & 0xF) * 17;
        alpha_table[i * 4 + 1] = (((*alpha_data) >> 4) & 0xF) * 17;
        alpha_table[i * 4 + 2] = (((*alpha_data) >> 8) & 0xF) * 17;
        alpha_table[i * 4 + 3] = (((*alpha_data) >> 12) & 0xF) * 17;

        block_storage += 2;
    }

    decompress_block_dxt1_shared(x, y, width, block_storage, image, alpha_table, false);
}

/**
 * \brief Decompresses one block of a DXT5 texture and stores the resulting pixels at the appropriate offset in 'image'.
 *
 * \param x                 x-coordinate of the first pixel in the block.
 * \param y                 y-coordinate of the first pixel in the block.
 * \param width             width of the texture being decompressed.
 * \param height            height of the texture being decompressed.
 * \param block_storage     pointer to the block to decompress.
 * \param image             pointer to image where the decompressed pixel data should be stored.
 **/
static void decompress_block_dxt5(std::uint32_t x, std::uint32_t y, std::uint32_t width, const std::uint8_t *block_storage, std::uint32_t *image) {
    std::uint8_t alpha0 = *reinterpret_cast<const std::uint8_t *>(block_storage);
    std::uint8_t alpha1 = *reinterpret_cast<const std::uint8_t *>(block_storage + 1);

    const std::uint8_t *bits = block_storage + 2;
    std::uint32_t alpha_code_1 = bits[2] | (bits[3] << 8) | (bits[4] << 16) | (bits[5] << 24);
    std::uint16_t alpha_code_2 = bits[0] | (bits[1] << 8);

    std::uint16_t color0 = *reinterpret_cast<const std::uint16_t *>(block_storage + 8);
    std::uint16_t color1 = *reinterpret_cast<const std::uint16_t *>(block_storage + 10);

    std::uint32_t temp;

    temp = (color0 >> 11) * 255 + 16;
    std::uint8_t r0 = (std::uint8_t)((temp / 32 + temp) / 32);
    temp = ((color0 & 0x07E0) >> 5) * 255 + 32;
    std::uint8_t g0 = (std::uint8_t)((temp / 64 + temp) / 64);
    temp = (color0 & 0x001F) * 255 + 16;
    std::uint8_t b0 = (std::uint8_t)((temp / 32 + temp) / 32);

    temp = (color1 >> 11) * 255 + 16;
    std::uint8_t r1 = (std::uint8_t)((temp / 32 + temp) / 32);
    temp = ((color1 & 0x07E0) >> 5) * 255 + 32;
    std::uint8_t g1 = (std::uint8_t)((temp / 64 + temp) / 64);
    temp = (color1 & 0x001F) * 255 + 16;
    std::uint8_t b1 = (std::uint8_t)((temp / 32 + temp) / 32);

    std::uint32_t code = *reinterpret_cast<const std::uint32_t *>(block_storage + 12);

    for (int j = 0; j < 4; j++) {
        for (int i = 0; i < 4; i++) {
            int alpha_code_index = 3 * (4 * j + i);
            int alphaCode;

            if (alpha_code_index <= 12) {
                alphaCode = (alpha_code_2 >> alpha_code_index) & 0x07;
            } else if (alpha_code_index == 15) {
                alphaCode = (alpha_code_2 >> 15) | ((alpha_code_1 << 1) & 0x06);
            } else {
                alphaCode = (alpha_code_1 >> (alpha_code_index - 16)) & 0x07;
            }

            std::uint8_t final_alpha;
            if (alphaCode == 0) {
                final_alpha = alpha0;
            } else if (alphaCode == 1) {
                final_alpha = alpha1;
            } else {
                if (alpha0 > alpha1) {
                    final_alpha = ((8 - alphaCode) * alpha0 + (alphaCode - 1) * alpha1) / 7;
                } else {
                    if (alphaCode == 6)
                        final_alpha = 0;
                    else if (alphaCode == 7)
                        final_alpha = 255;
                    else
                        final_alpha = ((6 - alphaCode) * alpha0 + (alphaCode - 1) * alpha1) / 5;
                }
            }

            std::uint8_t color_code = (code >> 2 * (4 * j + i)) & 0x03;

            std::uint32_t final_color;
            switch (color_code) {
            case 0:
                final_color = pack_rgba_reversed(r0, g0, b0, final_alpha);
                break;
            case 1:
                final_color = pack_rgba_reversed(r1, g1, b1, final_alpha);
                break;
            case 2:
                final_color = pack_rgba_reversed((2 * r0 + r1) / 3, (2 * g0 + g1) / 3, (2 * b0 + b1) / 3, final_alpha);
                break;
            case 3:
                final_color = pack_rgba_reversed((r0 + 2 * r1) / 3, (g0 + 2 * g1) / 3, (b0 + 2 * b1) / 3, final_alpha);
                break;
            }

            image[j * 4 + i] = final_color;
        }
    }
}

/**
 * \brief Decompresses all the blocks of a DXT compressed texture and stores the resulting pixels in 'image'.
 *
 * Output results is in format RGBA, with each channel being 8 bits.
 *
 * \param width            Texture width.
 * \param height           Texture height.
 * \param block_storage    Pointer to compressed DXT1 blocks.
 * \param image            Pointer to the image where the decompressed pixels will be stored.
 * \param bc_type          Block compressed type. BC1 (DXT1), BC2 (DXT2) or BC3 (DXT3).
 */
void decompress_bc_swizz_image(std::uint32_t width, std::uint32_t height, const std::uint8_t *block_storage, std::uint32_t *image, const std::uint8_t bc_type) {
    std::uint32_t block_count_x = (width + 3) / 4;
    std::uint32_t block_count_y = (height + 3) / 4;
    std::uint32_t block_size = (bc_type > 1) ? 16 : 8;

    std::uint32_t temp_block_result[16] = {};

    // This looks like swizzle order but it's not.
    static const int dxt_order[] = {
        0, 2, 8, 10,
        1, 3, 9, 11,
        4, 6, 12, 14,
        5, 7, 13, 15
    };

    for (std::uint32_t j = 0; j < block_count_y; j++) {
        for (std::uint32_t i = 0; i < block_count_x; i++) {
            switch (bc_type) {
            case 1:
                decompress_block_dxt1(i * 4, j * 4, width, block_storage + i * block_size, temp_block_result);
                break;

            case 2:
                decompress_block_dxt3(i * 4, j * 4, width, block_storage + i * block_size, temp_block_result);
                break;

            case 3:
                decompress_block_dxt5(i * 4, j * 4, width, block_storage + i * block_size, temp_block_result);
                break;
            }

            for (int b = 0; b < 16; b++) {
                image[dxt_order[b]] = temp_block_result[b];
            }

            image += 16;
        }

        block_storage += block_count_x * block_size;
    }
}

} // namespace renderer::texture
