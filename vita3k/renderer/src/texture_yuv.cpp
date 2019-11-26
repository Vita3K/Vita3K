#include <renderer/functions.h>

#include <util/log.h>

#include <algorithm>

extern "C" {
#include <libswscale/swscale.h>
}

namespace renderer::texture {
void yuv420_texture_to_rgb(uint8_t *dst, const uint8_t *src, size_t width, size_t height) {
    SwsContext *context = sws_getContext(width, height, AV_PIX_FMT_YUV420P, width, height, AV_PIX_FMT_RGB24,
        0, nullptr, nullptr, nullptr);
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
    sws_freeContext(context);
}
}
