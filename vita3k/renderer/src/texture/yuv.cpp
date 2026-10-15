// Vita3K emulator project
// Copyright (C) 2026 Vita3K team
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
#include <renderer/texture_cache.h>

#include <algorithm>

extern "C" {
#include <libswscale/swscale.h>
}

namespace renderer::texture {

static SwsContext *get_sws_context(YUVConversionCache &cache, size_t width, size_t height, bool is_p3) {
    bool recreate = false;
    auto *context = static_cast<SwsContext *>(cache.sws_context);
    if (cache.width != width || cache.height != height || cache.is_p3 != is_p3) {
        recreate = true;
        cache.width = width;
        cache.height = height;
        cache.is_p3 = is_p3;
    } else if (context == nullptr) {
        recreate = true;
    }

    if (recreate) {
        if (context != nullptr) {
            sws_freeContext(context);
            context = nullptr;
        }
        const AVPixelFormat format = is_p3 ? AV_PIX_FMT_YUV420P : AV_PIX_FMT_NV12;
        context = sws_getContext(width, height, format, width, height, AV_PIX_FMT_RGB0,
            0, nullptr, nullptr, nullptr);
        cache.sws_context = context;
    }
    return context;
}

void yuv420_texture_to_rgb(YUVConversionCache &cache, uint8_t *dst, const uint8_t *src, uint32_t width, uint32_t height, uint32_t layout_width, uint32_t layout_height, bool is_p3) {
    SwsContext *context = get_sws_context(cache, width, height, is_p3);
    assert(context);

    const uint8_t *slices[] = {
        src, // Y Slice
        src + layout_width * layout_height, // U(V for P2) Slice
        src + layout_width * layout_height + layout_width * layout_height / 4, // V Slice (for P3)
    };

    int strides[] = {
        static_cast<int>(width),
        static_cast<int>(width / 2),
        static_cast<int>(width / 2),
    };
    if (!is_p3) {
        // src only have two slices
        strides[1] = static_cast<int>(width);
        strides[2] = 0;
    }

    uint8_t *dst_slices[] = {
        dst,
    };

    const int dst_strides[] = {
        static_cast<int>(width * 4),
    };

    int error = sws_scale(context, slices, strides, 0, height, dst_slices, dst_strides);
    assert(error == height);
}

// Packed 4:2:2, two pixels in 4 bytes. CSC0 is BT.601 and CSC1 BT.709, both video range (as the
// YUV420 path above, which uses swscale's default BT.601 video range).
void yuv422_texture_to_rgba(uint8_t *dst, const uint8_t *src, uint32_t width, uint32_t height, SceGxmTextureFormat fmt) {
    const auto mode = static_cast<SceGxmTextureSwizzleYUV422Mode>(fmt & SCE_GXM_TEXTURE_SWIZZLE_MASK);
    const bool bt709 = mode >= SCE_GXM_TEXTURE_SWIZZLE_YUYV_CSC1;
    // byte offsets of Y0, Y1, U and V in each 4-byte pair
    int y0, y1, u, v;
    switch (mode) {
    case SCE_GXM_TEXTURE_SWIZZLE_YUYV_CSC0:
    case SCE_GXM_TEXTURE_SWIZZLE_YUYV_CSC1:
        y0 = 0, u = 1, y1 = 2, v = 3;
        break;
    case SCE_GXM_TEXTURE_SWIZZLE_YVYU_CSC0:
    case SCE_GXM_TEXTURE_SWIZZLE_YVYU_CSC1:
        y0 = 0, v = 1, y1 = 2, u = 3;
        break;
    case SCE_GXM_TEXTURE_SWIZZLE_UYVY_CSC0:
    case SCE_GXM_TEXTURE_SWIZZLE_UYVY_CSC1:
        u = 0, y0 = 1, v = 2, y1 = 3;
        break;
    default: // VYUY
        v = 0, y0 = 1, u = 2, y1 = 3;
        break;
    }
    // 16.16 fixed point coefficients: Y scale, V->R, U->G, V->G, U->B
    const int ky = 76309; // 255 / 219
    const int kvr = bt709 ? 117504 : 104597;
    const int kug = bt709 ? 13954 : 25675;
    const int kvg = bt709 ? 34903 : 53279;
    const int kub = bt709 ? 138453 : 132201;
    const auto clamp8 = [](int x) { return static_cast<uint8_t>(std::clamp(x >> 16, 0, 255)); };
    const auto pixel = [&](uint8_t *out, int y, int cu, int cv) {
        const int luma = (y - 16) * ky + 32768;
        out[0] = clamp8(luma + kvr * cv);
        out[1] = clamp8(luma - kug * cu - kvg * cv);
        out[2] = clamp8(luma + kub * cu);
        out[3] = 255;
    };
    for (uint32_t row = 0; row < height; row++) {
        const uint8_t *in = src + static_cast<size_t>(row) * width * 2;
        uint8_t *out = dst + static_cast<size_t>(row) * width * 4;
        for (uint32_t x = 0; x + 1 < width; x += 2, in += 4, out += 8) {
            const int cu = in[u] - 128;
            const int cv = in[v] - 128;
            pixel(out, in[y0], cu, cv);
            pixel(out + 4, in[y1], cu, cv);
        }
    }
}

} // namespace renderer::texture

renderer::TextureCache::~TextureCache() {
    if (auto *context = static_cast<SwsContext *>(yuv_conversion_cache.sws_context)) {
        sws_freeContext(context);
        yuv_conversion_cache.sws_context = nullptr;
    }
}
