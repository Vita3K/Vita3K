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

} // namespace renderer::texture

renderer::TextureCache::~TextureCache() {
    if (auto *context = static_cast<SwsContext *>(yuv_conversion_cache.sws_context)) {
        sws_freeContext(context);
        yuv_conversion_cache.sws_context = nullptr;
    }
}
