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

#include <gxm/functions.h>
#include <map>

namespace gxm {
size_t get_width(const SceGxmTexture *texture) {
    if ((texture->type << 29 != SCE_GXM_TEXTURE_SWIZZLED) && (texture->type << 29 != SCE_GXM_TEXTURE_CUBE)) {
        return texture->width + 1;
    }
    return 1ull << (texture->width_base2 & 0xF);
}

size_t get_height(const SceGxmTexture *texture) {
    if ((texture->type << 29 != SCE_GXM_TEXTURE_SWIZZLED) && (texture->type << 29 != SCE_GXM_TEXTURE_CUBE)) {
        return texture->height + 1;
    }
    return 1ull << (texture->height_base2 & 0xF);
}

SceGxmTextureFormat get_format(const SceGxmTexture *texture) {
    return static_cast<SceGxmTextureFormat>(
        texture->base_format << 24 | texture->format0 << 31 | texture->swizzle_format << 12);
}

SceGxmTextureBaseFormat get_base_format(SceGxmTextureFormat src) {
    return static_cast<SceGxmTextureBaseFormat>(src & SCE_GXM_TEXTURE_BASE_FORMAT_MASK);
}

size_t get_stride_in_bytes(const SceGxmTexture *texture) {
    return ((texture->mip_filter | (texture->min_filter << 1) | (texture->mip_count << 3) | (texture->lod_bias << 7)) + 1) * 4;
}

bool is_block_compressed_format(SceGxmTextureBaseFormat base_format) {
    return ((base_format == SCE_GXM_TEXTURE_BASE_FORMAT_PVRT2BPP)
        || (base_format == SCE_GXM_TEXTURE_BASE_FORMAT_PVRT4BPP)
        || (base_format == SCE_GXM_TEXTURE_BASE_FORMAT_PVRTII2BPP)
        || (base_format == SCE_GXM_TEXTURE_BASE_FORMAT_PVRTII4BPP)
        || (base_format == SCE_GXM_TEXTURE_BASE_FORMAT_UBC1)
        || (base_format == SCE_GXM_TEXTURE_BASE_FORMAT_UBC2)
        || (base_format == SCE_GXM_TEXTURE_BASE_FORMAT_UBC3)
        || (base_format == SCE_GXM_TEXTURE_BASE_FORMAT_UBC4)
        || (base_format == SCE_GXM_TEXTURE_BASE_FORMAT_SBC4)
        || (base_format == SCE_GXM_TEXTURE_BASE_FORMAT_UBC5)
        || (base_format == SCE_GXM_TEXTURE_BASE_FORMAT_SBC5));
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
