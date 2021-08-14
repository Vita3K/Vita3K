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

#include <util/log.h>

#include <algorithm>

extern "C" {
#include <libswscale/swscale.h>
}

namespace renderer::texture {

static SwsContext *s_render_sws_context{};
static size_t res[2] = { 0, 0 };
SwsContext *get_sws_context(size_t width, size_t height) {
    bool recreate = false;
    if (res[0] != width || res[1] != height) {
        recreate = true;
        res[0] = width;
        res[1] = height;
    } else if (s_render_sws_context == nullptr) {
        recreate = true;
    }

    if (recreate) {
        if (s_render_sws_context != nullptr) {
            sws_freeContext(s_render_sws_context);
            s_render_sws_context = nullptr;
        }
        s_render_sws_context = sws_getContext(width, height, AV_PIX_FMT_YUV420P, width, height, AV_PIX_FMT_RGB24,
            0, nullptr, nullptr, nullptr);
    }
    return s_render_sws_context;
}

void yuv420_texture_to_rgb(uint8_t *dst, const uint8_t *src, size_t width, size_t height) {
    SwsContext *context = get_sws_context(width, height);
    assert(context);

    const uint8_t *slices[] = {
        &src[0], // Y Slice
        &src[width * height], // U Slice
        &src[width * height + width * height / 4], // V Slice
    };

    int strides[] = {
        static_cast<int>(width),
        static_cast<int>(width / 2),
        static_cast<int>(width / 2),
    };

    uint8_t *dst_slices[] = {
        dst,
    };

    const int dst_strides[] = {
        static_cast<int>(width * 3),
    };

    int error = sws_scale(context, slices, strides, 0, height, dst_slices, dst_strides);
    assert(error == height);
}
} // namespace renderer::texture
