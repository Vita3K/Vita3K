// Vita3K emulator project
// Copyright (C) 2025 Vita3K team
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

#include <gxm/types.h>
#include <util/align.h>

#include <bit>
#include <cassert>
#include <map>

namespace gxm {
uint32_t get_width(const SceGxmTexture &texture) {
    if (texture.texture_type() != SCE_GXM_TEXTURE_SWIZZLED && texture.texture_type() != SCE_GXM_TEXTURE_CUBE) {
        return texture.width + 1;
    }
    return 1u << texture.width_base2;
}

uint32_t get_height(const SceGxmTexture &texture) {
    if (texture.texture_type() != SCE_GXM_TEXTURE_SWIZZLED && texture.texture_type() != SCE_GXM_TEXTURE_CUBE) {
        return texture.height + 1;
    }
    return 1u << texture.height_base2;
}

SceGxmTextureFormat get_format(const SceGxmTexture &texture) {
    return static_cast<SceGxmTextureFormat>(
        texture.base_format << 24 | texture.format0 << 31 | texture.swizzle_format << 12);
}

SceGxmTextureBaseFormat get_base_format(SceGxmTextureFormat src) {
    return static_cast<SceGxmTextureBaseFormat>(src & SCE_GXM_TEXTURE_BASE_FORMAT_MASK);
}

uint32_t get_num_components(SceGxmTextureBaseFormat fmt) {
    switch (fmt) {
    case SCE_GXM_TEXTURE_BASE_FORMAT_U8:
    case SCE_GXM_TEXTURE_BASE_FORMAT_S8:
    case SCE_GXM_TEXTURE_BASE_FORMAT_U16:
    case SCE_GXM_TEXTURE_BASE_FORMAT_S16:
    case SCE_GXM_TEXTURE_BASE_FORMAT_F16:
    case SCE_GXM_TEXTURE_BASE_FORMAT_U32:
    case SCE_GXM_TEXTURE_BASE_FORMAT_S32:
    case SCE_GXM_TEXTURE_BASE_FORMAT_F32:
    case SCE_GXM_TEXTURE_BASE_FORMAT_F32M:
    case SCE_GXM_TEXTURE_BASE_FORMAT_UBC4:
    case SCE_GXM_TEXTURE_BASE_FORMAT_SBC4:
    case SCE_GXM_TEXTURE_BASE_FORMAT_X8U24:
        return 1;

    case SCE_GXM_TEXTURE_BASE_FORMAT_U8U8:
    case SCE_GXM_TEXTURE_BASE_FORMAT_S8S8:
    case SCE_GXM_TEXTURE_BASE_FORMAT_U16U16:
    case SCE_GXM_TEXTURE_BASE_FORMAT_S16S16:
    case SCE_GXM_TEXTURE_BASE_FORMAT_F16F16:
    case SCE_GXM_TEXTURE_BASE_FORMAT_U32U32:
    case SCE_GXM_TEXTURE_BASE_FORMAT_F32F32:
    case SCE_GXM_TEXTURE_BASE_FORMAT_UBC5:
    case SCE_GXM_TEXTURE_BASE_FORMAT_SBC5:
        return 2;

    // 3 components
    case SCE_GXM_TEXTURE_BASE_FORMAT_U8U8U8:
    case SCE_GXM_TEXTURE_BASE_FORMAT_S8S8S8:
    case SCE_GXM_TEXTURE_BASE_FORMAT_F11F11F10:
    case SCE_GXM_TEXTURE_BASE_FORMAT_SE5M9M9M9:
    case SCE_GXM_TEXTURE_BASE_FORMAT_U5U6U5:
    case SCE_GXM_TEXTURE_BASE_FORMAT_S5S5U6:
    case SCE_GXM_TEXTURE_BASE_FORMAT_X8S8S8U8:
    case SCE_GXM_TEXTURE_BASE_FORMAT_YUV420P2:
    case SCE_GXM_TEXTURE_BASE_FORMAT_YUV420P3:
    case SCE_GXM_TEXTURE_BASE_FORMAT_YUV422:
        return 3;

    // 4 components
    case SCE_GXM_TEXTURE_BASE_FORMAT_U8U3U3U2:
    case SCE_GXM_TEXTURE_BASE_FORMAT_U8U8U8U8:
    case SCE_GXM_TEXTURE_BASE_FORMAT_S8S8S8S8:
    case SCE_GXM_TEXTURE_BASE_FORMAT_U16U16U16U16:
    case SCE_GXM_TEXTURE_BASE_FORMAT_S16S16S16S16:
    case SCE_GXM_TEXTURE_BASE_FORMAT_F16F16F16F16:
    case SCE_GXM_TEXTURE_BASE_FORMAT_P8:
    case SCE_GXM_TEXTURE_BASE_FORMAT_P4:
    case SCE_GXM_TEXTURE_BASE_FORMAT_PVRT2BPP:
    case SCE_GXM_TEXTURE_BASE_FORMAT_PVRT4BPP:
    case SCE_GXM_TEXTURE_BASE_FORMAT_PVRTII2BPP:
    case SCE_GXM_TEXTURE_BASE_FORMAT_PVRTII4BPP:
    case SCE_GXM_TEXTURE_BASE_FORMAT_U2U10U10U10:
    case SCE_GXM_TEXTURE_BASE_FORMAT_U2F10F10F10:
    case SCE_GXM_TEXTURE_BASE_FORMAT_UBC1:
    case SCE_GXM_TEXTURE_BASE_FORMAT_UBC2:
    case SCE_GXM_TEXTURE_BASE_FORMAT_UBC3:
    case SCE_GXM_TEXTURE_BASE_FORMAT_U4U4U4U4:
    case SCE_GXM_TEXTURE_BASE_FORMAT_U1U5U5U5:
        return 4;

    default:
        return 0;
    }
}

uint32_t get_stride_in_bytes(const SceGxmTexture &texture) {
    assert(texture.texture_type() == SCE_GXM_TEXTURE_LINEAR_STRIDED);
    return ((texture.mip_filter | (texture.min_filter << 1) | (texture.mip_count << 3) | (texture.lod_bias << 7)) + 1) * 4;
}

bool is_bcn_format(SceGxmTextureBaseFormat base_format) {
    return base_format == SCE_GXM_TEXTURE_BASE_FORMAT_UBC1
        || base_format == SCE_GXM_TEXTURE_BASE_FORMAT_UBC2
        || base_format == SCE_GXM_TEXTURE_BASE_FORMAT_UBC3
        || base_format == SCE_GXM_TEXTURE_BASE_FORMAT_UBC4
        || base_format == SCE_GXM_TEXTURE_BASE_FORMAT_SBC4
        || base_format == SCE_GXM_TEXTURE_BASE_FORMAT_UBC5
        || base_format == SCE_GXM_TEXTURE_BASE_FORMAT_SBC5
        || base_format == SCE_GXM_TEXTURE_BASE_FORMAT_UBC6H
        || base_format == SCE_GXM_TEXTURE_BASE_FORMAT_SBC6H
        || base_format == SCE_GXM_TEXTURE_BASE_FORMAT_UBC7;
}

bool is_pvrt_format(SceGxmTextureBaseFormat base_format) {
    return base_format == SCE_GXM_TEXTURE_BASE_FORMAT_PVRT2BPP
        || base_format == SCE_GXM_TEXTURE_BASE_FORMAT_PVRT4BPP
        || base_format == SCE_GXM_TEXTURE_BASE_FORMAT_PVRTII2BPP
        || base_format == SCE_GXM_TEXTURE_BASE_FORMAT_PVRTII4BPP;
}

bool is_block_compressed_format(SceGxmTextureBaseFormat base_format) {
    return is_bcn_format(base_format) || is_pvrt_format(base_format);
}

uint32_t bits_per_pixel(SceGxmTextureBaseFormat base_format) {
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
    case SCE_GXM_TEXTURE_BASE_FORMAT_U2F10F10F10:
        return 32;
    case SCE_GXM_TEXTURE_BASE_FORMAT_F16F16F16F16:
    case SCE_GXM_TEXTURE_BASE_FORMAT_U16U16U16U16:
    case SCE_GXM_TEXTURE_BASE_FORMAT_S16S16S16S16:
    case SCE_GXM_TEXTURE_BASE_FORMAT_F32F32:
    case SCE_GXM_TEXTURE_BASE_FORMAT_U32U32:
        return 64;
    case SCE_GXM_TEXTURE_BASE_FORMAT_PVRT2BPP:
    case SCE_GXM_TEXTURE_BASE_FORMAT_PVRTII2BPP:
        return 2;
    case SCE_GXM_TEXTURE_BASE_FORMAT_PVRT4BPP:
    case SCE_GXM_TEXTURE_BASE_FORMAT_PVRTII4BPP:
        return 4;
    case SCE_GXM_TEXTURE_BASE_FORMAT_UBC1:
    case SCE_GXM_TEXTURE_BASE_FORMAT_UBC4:
    case SCE_GXM_TEXTURE_BASE_FORMAT_SBC4:
        return 4;
    case SCE_GXM_TEXTURE_BASE_FORMAT_UBC2:
    case SCE_GXM_TEXTURE_BASE_FORMAT_UBC3:
    case SCE_GXM_TEXTURE_BASE_FORMAT_UBC5:
    case SCE_GXM_TEXTURE_BASE_FORMAT_SBC5:
    case SCE_GXM_TEXTURE_BASE_FORMAT_UBC6H:
    case SCE_GXM_TEXTURE_BASE_FORMAT_SBC6H:
    case SCE_GXM_TEXTURE_BASE_FORMAT_UBC7:
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
    default:
        // we can't use an integer for astc textures
        return 1;
    }
}

std::pair<uint32_t, uint32_t> get_block_size(SceGxmTextureBaseFormat base_format) {
    switch (base_format) {
    case SCE_GXM_TEXTURE_BASE_FORMAT_UBC1:
    case SCE_GXM_TEXTURE_BASE_FORMAT_UBC2:
    case SCE_GXM_TEXTURE_BASE_FORMAT_UBC3:
    case SCE_GXM_TEXTURE_BASE_FORMAT_UBC4:
    case SCE_GXM_TEXTURE_BASE_FORMAT_SBC4:
    case SCE_GXM_TEXTURE_BASE_FORMAT_UBC5:
    case SCE_GXM_TEXTURE_BASE_FORMAT_SBC5:
    case SCE_GXM_TEXTURE_BASE_FORMAT_UBC6H:
    case SCE_GXM_TEXTURE_BASE_FORMAT_SBC6H:
    case SCE_GXM_TEXTURE_BASE_FORMAT_UBC7:
        return { 4, 4 };

    case SCE_GXM_TEXTURE_BASE_FORMAT_PVRT2BPP:
    case SCE_GXM_TEXTURE_BASE_FORMAT_PVRTII2BPP:
        return { 8, 4 };

    case SCE_GXM_TEXTURE_BASE_FORMAT_PVRT4BPP:
    case SCE_GXM_TEXTURE_BASE_FORMAT_PVRTII4BPP:
        return { 4, 4 };

    case SCE_GXM_TEXTURE_BASE_FORMAT_P4:
        return { 4, 4 };

    case SCE_GXM_TEXTURE_BASE_FORMAT_YUV422:
        return { 2, 1 };

    case SCE_GXM_TEXTURE_BASE_FORMAT_YUV420P2:
    case SCE_GXM_TEXTURE_BASE_FORMAT_YUV420P3:
        return { 2, 2 };
#define ASTC_FMT(b_x, b_y)                              \
    case SCE_GXM_TEXTURE_BASE_FORMAT_ASTC##b_x##x##b_y: \
        return { b_x, b_y };

        // Note: not the cleanest thing to do, but anyway
#include "../../renderer/src/texture/astc_formats.inc"
#undef ASTC_FMT

    default:
        return { 1, 1 };
    }
}

uint32_t texture_size_first_mip(const SceGxmTexture &texture) {
    SceGxmTextureType type = texture.texture_type();
    const SceGxmTextureBaseFormat format = get_base_format(get_format(texture));

    if (format == SCE_GXM_TEXTURE_BASE_FORMAT_YUV420P2 || format == SCE_GXM_TEXTURE_BASE_FORMAT_YUV420P3) {
        // multiplane formats must be handled differently
        assert(type == SCE_GXM_TEXTURE_LINEAR);

        uint32_t width = align(get_width(texture), 8);
        uint32_t height = get_height(texture);

        if (texture.mip_count != 0xF) {
            // offset between planes is by rounding the width and height
            width = next_power_of_two(width);
            height = next_power_of_two(height);
        }

        // YUV420 is 12bpp
        return (width * height * 3) / 2;
    }

    if (type == SCE_GXM_TEXTURE_LINEAR_STRIDED) {
        return get_stride_in_bytes(texture) * get_height(texture);
    }

    uint32_t width = get_width(texture);
    uint32_t height = get_height(texture);

    switch (type) {
    case SCE_GXM_TEXTURE_SWIZZLED_ARBITRARY:
    case SCE_GXM_TEXTURE_CUBE_ARBITRARY:
        // turn arbitrary types into non-arbitrary with their width/height rounded up
        // that's how they are stored in memory anyway
        width = next_power_of_two(width);
        height = next_power_of_two(height);
        break;
    case SCE_GXM_TEXTURE_TILED:
        width = align(width, 32);
        height = align(height, 32);
        break;
    case SCE_GXM_TEXTURE_LINEAR:
        width = align(width, 8);
        break;
    default:
        break;
    }

    auto [block_width, block_height] = get_block_size(format);
    width = align(width, block_width);
    height = align(height, block_height);

    const uint32_t bpp = bits_per_pixel(format);
    // block size in bytes
    const uint32_t block_size = (block_width * block_height * bpp) / 8;

    // from the number of pixels in a mipmap, we can get the number of blocks by shifting to the right by block_shift
    const uint32_t block_shift = std::bit_width(block_width * block_height) - 1;

    return ((width * height) >> block_shift) * block_size;
}

bool is_paletted_format(SceGxmTextureBaseFormat base_format) {
    return base_format == SCE_GXM_TEXTURE_BASE_FORMAT_P8 || base_format == SCE_GXM_TEXTURE_BASE_FORMAT_P4;
}

bool is_yuv_format(SceGxmTextureBaseFormat base_format) {
    return ((base_format == SCE_GXM_TEXTURE_BASE_FORMAT_YUV420P2)
        || (base_format == SCE_GXM_TEXTURE_BASE_FORMAT_YUV420P3)
        || (base_format == SCE_GXM_TEXTURE_BASE_FORMAT_YUV422));
}

bool convert_color_format_to_texture_format(SceGxmColorFormat format, SceGxmTextureFormat &dest_format) {
    static const std::map<std::uint32_t, std::uint32_t> COLOR_FORMAT_TO_TEXTURE_MAPPING = {
        { SCE_GXM_COLOR_FORMAT_U8U8U8U8_ABGR, SCE_GXM_TEXTURE_FORMAT_U8U8U8U8_ABGR },
        { SCE_GXM_COLOR_FORMAT_U8U8U8U8_ARGB, SCE_GXM_TEXTURE_FORMAT_U8U8U8U8_ARGB },
        { SCE_GXM_COLOR_FORMAT_U8U8U8U8_RGBA, SCE_GXM_TEXTURE_FORMAT_U8U8U8U8_RGBA },
        { SCE_GXM_COLOR_FORMAT_U8U8U8U8_BGRA, SCE_GXM_TEXTURE_FORMAT_U8U8U8U8_BGRA },
        { SCE_GXM_COLOR_FORMAT_U8U8U8_BGR, SCE_GXM_TEXTURE_FORMAT_U8U8U8_BGR },
        { SCE_GXM_COLOR_FORMAT_U8U8U8_RGB, SCE_GXM_TEXTURE_FORMAT_U8U8U8_RGB },
        { SCE_GXM_COLOR_FORMAT_U5U6U5_BGR, SCE_GXM_TEXTURE_FORMAT_U5U6U5_BGR },
        { SCE_GXM_COLOR_FORMAT_U5U6U5_RGB, SCE_GXM_TEXTURE_FORMAT_U5U6U5_RGB },
        { SCE_GXM_COLOR_FORMAT_U1U5U5U5_ABGR, SCE_GXM_TEXTURE_FORMAT_U1U5U5U5_ABGR },
        { SCE_GXM_COLOR_FORMAT_U1U5U5U5_ARGB, SCE_GXM_TEXTURE_FORMAT_U1U5U5U5_ARGB },
        { SCE_GXM_COLOR_FORMAT_U5U5U5U1_RGBA, SCE_GXM_TEXTURE_FORMAT_U5U5U5U1_RGBA },
        { SCE_GXM_COLOR_FORMAT_U5U5U5U1_BGRA, SCE_GXM_TEXTURE_FORMAT_U5U5U5U1_BGRA },
        { SCE_GXM_COLOR_FORMAT_U4U4U4U4_ABGR, SCE_GXM_TEXTURE_FORMAT_U4U4U4U4_ABGR },
        { SCE_GXM_COLOR_FORMAT_U4U4U4U4_ARGB, SCE_GXM_TEXTURE_FORMAT_U4U4U4U4_ARGB },
        { SCE_GXM_COLOR_FORMAT_U4U4U4U4_RGBA, SCE_GXM_TEXTURE_FORMAT_U4U4U4U4_RGBA },
        { SCE_GXM_COLOR_FORMAT_U4U4U4U4_BGRA, SCE_GXM_TEXTURE_FORMAT_U4U4U4U4_BGRA },
        { SCE_GXM_COLOR_BASE_FORMAT_U8U3U3U2, SCE_GXM_TEXTURE_FORMAT_U8U3U3U2_ARGB },
        { SCE_GXM_COLOR_FORMAT_F16_R, SCE_GXM_TEXTURE_FORMAT_F16_R },
        { SCE_GXM_COLOR_FORMAT_F16F16_GR, SCE_GXM_TEXTURE_FORMAT_F16F16_GR },
        { SCE_GXM_COLOR_FORMAT_F32_R, SCE_GXM_TEXTURE_FORMAT_F32_R },
        { SCE_GXM_COLOR_FORMAT_S16_R, SCE_GXM_TEXTURE_FORMAT_S16_R },
        { SCE_GXM_COLOR_FORMAT_S16S16_GR, SCE_GXM_TEXTURE_FORMAT_S16S16_GR },
        { SCE_GXM_COLOR_FORMAT_U16_R, SCE_GXM_TEXTURE_FORMAT_U16_R },
        { SCE_GXM_COLOR_FORMAT_U16U16_GR, SCE_GXM_TEXTURE_FORMAT_U16U16_GR },
        { SCE_GXM_COLOR_FORMAT_U2U10U10U10_ABGR, SCE_GXM_TEXTURE_FORMAT_U2U10U10U10_ABGR },
        { SCE_GXM_COLOR_FORMAT_U2U10U10U10_ARGB, SCE_GXM_TEXTURE_FORMAT_U2U10U10U10_ARGB },
        { SCE_GXM_COLOR_FORMAT_U10U10U10U2_RGBA, SCE_GXM_TEXTURE_FORMAT_U10U10U10U2_RGBA },
        { SCE_GXM_COLOR_FORMAT_U10U10U10U2_BGRA, SCE_GXM_TEXTURE_FORMAT_U10U10U10U2_BGRA },
        { SCE_GXM_COLOR_FORMAT_U8_R, SCE_GXM_TEXTURE_FORMAT_U8_R },
        { SCE_GXM_COLOR_FORMAT_U8_A, SCE_GXM_TEXTURE_FORMAT_U8_R },
        { SCE_GXM_COLOR_FORMAT_S8_R, SCE_GXM_TEXTURE_FORMAT_S8_R },
        { SCE_GXM_COLOR_FORMAT_S5S5U6_RGB, SCE_GXM_TEXTURE_FORMAT_S5S5U6_RGB },
        { SCE_GXM_COLOR_FORMAT_U8U8_GR, SCE_GXM_TEXTURE_FORMAT_U8U8_GR },
        { SCE_GXM_COLOR_FORMAT_S8S8_GR, SCE_GXM_TEXTURE_FORMAT_S8S8_GR },
        { SCE_GXM_COLOR_FORMAT_S8S8S8S8_BGRA, SCE_GXM_TEXTURE_FORMAT_S8S8S8S8_BGRA },
        { SCE_GXM_COLOR_FORMAT_S8S8S8S8_RGBA, SCE_GXM_TEXTURE_FORMAT_S8S8S8S8_RGBA },
        { SCE_GXM_COLOR_FORMAT_S8S8S8S8_ABGR, SCE_GXM_TEXTURE_FORMAT_S8S8S8S8_ABGR },
        { SCE_GXM_COLOR_FORMAT_S8S8S8S8_ARGB, SCE_GXM_TEXTURE_FORMAT_S8S8S8S8_ARGB },
        { SCE_GXM_COLOR_FORMAT_F16F16F16F16_ABGR, SCE_GXM_TEXTURE_FORMAT_F16F16F16F16_ABGR },
        { SCE_GXM_COLOR_FORMAT_F16F16F16F16_ARGB, SCE_GXM_TEXTURE_FORMAT_F16F16F16F16_ARGB },
        { SCE_GXM_COLOR_FORMAT_F16F16F16F16_RGBA, SCE_GXM_TEXTURE_FORMAT_F16F16F16F16_RGBA },
        { SCE_GXM_COLOR_FORMAT_F16F16F16F16_BGRA, SCE_GXM_TEXTURE_FORMAT_F16F16F16F16_BGRA },
        { SCE_GXM_COLOR_FORMAT_F32F32_GR, SCE_GXM_TEXTURE_FORMAT_F32F32_GR },
        { SCE_GXM_COLOR_FORMAT_F11F11F10_RGB, SCE_GXM_TEXTURE_FORMAT_F11F11F10_RGB },
        { SCE_GXM_COLOR_FORMAT_F10F11F11_BGR, SCE_GXM_TEXTURE_FORMAT_F10F11F11_BGR },
        { SCE_GXM_COLOR_FORMAT_SE5M9M9M9_BGR, SCE_GXM_TEXTURE_FORMAT_SE5M9M9M9_BGR },
        { SCE_GXM_COLOR_FORMAT_SE5M9M9M9_RGB, SCE_GXM_TEXTURE_FORMAT_SE5M9M9M9_RGB },
        { SCE_GXM_COLOR_FORMAT_U2F10F10F10_ABGR, SCE_GXM_TEXTURE_FORMAT_U2F10F10F10_ABGR },
        { SCE_GXM_COLOR_FORMAT_U2F10F10F10_ARGB, SCE_GXM_TEXTURE_FORMAT_U2F10F10F10_ARGB },
        { SCE_GXM_COLOR_FORMAT_F10F10F10U2_RGBA, SCE_GXM_TEXTURE_FORMAT_F10F10F10U2_RGBA },
        { SCE_GXM_COLOR_FORMAT_F10F10F10U2_BGRA, SCE_GXM_TEXTURE_FORMAT_F10F10F10U2_BGRA }
    };

    auto ite = COLOR_FORMAT_TO_TEXTURE_MAPPING.find(format);
    if (ite == COLOR_FORMAT_TO_TEXTURE_MAPPING.end())
        return false;

    dest_format = static_cast<SceGxmTextureFormat>(ite->second);
    return true;
}
} // namespace gxm
