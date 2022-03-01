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
} // namespace gxm
