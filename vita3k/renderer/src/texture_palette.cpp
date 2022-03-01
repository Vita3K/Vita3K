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

#include <gxm/types.h>
#include <mem/ptr.h>
#include <util/log.h>

namespace renderer {
namespace texture {

void palette_texture_to_rgba_4(uint32_t *dst, const uint8_t *src, size_t width, size_t height, const uint32_t *palette) {
    R_PROFILE(__func__);

    const size_t stride = ((width + 7) & ~7) / 2; // NOTE: This is correct only with linear textures.
    for (size_t y = 0; y < height; ++y) {
        uint32_t *const dst_row = &dst[y * width];
        const uint8_t *const src_row = &src[y * stride];
        for (size_t x = 0; x < width; x += 2) {
            const uint8_t lohi = src_row[x / 2];
            const uint8_t lo = lohi & 0xf;
            const uint8_t hi = lohi >> 4;
            dst_row[x + 0] = palette[lo];
            dst_row[x + 1] = palette[hi];
        }
    }
}

void palette_texture_to_rgba_8(uint32_t *dst, const uint8_t *src, size_t width, size_t height, const uint32_t *palette) {
    R_PROFILE(__func__);

    const size_t stride = (width + 7) & ~7; // NOTE: This is correct only with linear textures.
    for (size_t y = 0; y < height; ++y) {
        uint32_t *const dst_row = &dst[y * width];
        const uint8_t *const src_row = &src[y * stride];
        for (size_t x = 0; x < width; ++x) {
            dst_row[x] = palette[src_row[x]];
        }
    }
}

const uint32_t *get_texture_palette(const SceGxmTexture &texture, const MemState &mem) {
    const Ptr<const uint32_t> palette_ptr(texture.palette_addr << 6);
    return palette_ptr.get(mem);
}

} // namespace texture
} // namespace renderer
