#include <renderer/functions.h>

#include "profile.h"

#include <mem/ptr.h>
#include <util/log.h>
#include <gxm/types.h>

namespace renderer::gl {
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

const uint32_t *get_texture_palette(const emu::SceGxmTexture &texture, const MemState &mem) {
    const Ptr<const uint32_t> palette_ptr(texture.palette_addr << 6);
    return palette_ptr.get(mem);
}

} // namespace texture
} // namespace renderer
