#include <renderer/functions.h>

#include <util/log.h>

#include <vector>
#include <algorithm>

namespace renderer::texture {
struct Color {
    uint8_t x = 0;
    uint8_t y = 0;
    uint8_t z = 0;
};

static Color yuv_color_to_rgb(Color yuv) {
    double y = (double)yuv.x / 255.0f;
    double u = (double)yuv.y / 255.0f;
    double v = (double)yuv.z / 255.0f;

    y = 1.1643 * (y - 0.0625);
    u = u - 0.5;
    v = v - 0.5;

    double r = std::min(std::max(y + 1.5958 * v, 0.0), 1.0);
    double g = std::min(std::max(y - 0.39173 * u - 0.8129 * v, 0.0), 1.0);
    double b = std::min(std::max(y + 2.017 * u, 0.0), 1.0);

    Color result;
    result.x = r * 255;
    result.y = g * 255;
    result.z = b * 255;

    return result;
}

void yuv420_texture_to_rgb(uint8_t *dst, const uint8_t *src, size_t width, size_t height) {
    auto *dst_colors = reinterpret_cast<Color *>(dst);
    std::vector<Color> yuvLayout(width * height);

    auto *yData = (uint8_t *)&src[0];
    auto *uData = (uint8_t *)&src[width * height];
    auto *vData = (uint8_t *)&src[width * height + width * height / 4];

    for (size_t y = 0; y < height; y++) {
        for (size_t x = 0; x < width; x++) {
            yuvLayout[x + y * width].x = yData[x + y * width];
        }
    }

    for (size_t y = 0; y < height / 2; y++) {
        for (size_t x = 0; x < width / 2; x++) {
            uint8_t u = uData[x + y * width / 2];
            uint8_t v = vData[x + y * width / 2];

            yuvLayout[(x * 2) + (y * 2 * width)].y = u;
            yuvLayout[(x * 2) + (y * 2 * width)].z = v;
            yuvLayout[(x * 2 + 1) + (y * 2 * width)].y = u;
            yuvLayout[(x * 2 + 1) + (y * 2 * width)].z = v;
            yuvLayout[(x * 2) + ((y * 2 + 1) * width)].y = u;
            yuvLayout[(x * 2) + ((y * 2 + 1) * width)].z = v;
            yuvLayout[(x * 2 + 1) + ((y * 2 + 1) * width)].y = u;
            yuvLayout[(x * 2 + 1) + ((y * 2 + 1) * width)].z = v;
        }
    }

    for (size_t a = 0; a < yuvLayout.size(); a++) {
        dst_colors[a] = yuv_color_to_rgb(yuvLayout[a]);
    }
}
}
