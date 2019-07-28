#include <cmath>
#include <cstring>
#include <psp2/gxm.h>

namespace renderer::texture {

size_t bits_per_pixel(SceGxmTextureBaseFormat base_format) {
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
        return 32;
    case SCE_GXM_TEXTURE_BASE_FORMAT_F16F16F16F16:
    case SCE_GXM_TEXTURE_BASE_FORMAT_U16U16U16U16:
    case SCE_GXM_TEXTURE_BASE_FORMAT_S16S16S16S16:
    case SCE_GXM_TEXTURE_BASE_FORMAT_F32F32:
    case SCE_GXM_TEXTURE_BASE_FORMAT_U32U32:
        return 64;
    case SCE_GXM_TEXTURE_BASE_FORMAT_PVRT2BPP:
        return 2;
    case SCE_GXM_TEXTURE_BASE_FORMAT_PVRT4BPP:
        return 4;
    case SCE_GXM_TEXTURE_BASE_FORMAT_PVRTII2BPP:
        return 2;
    case SCE_GXM_TEXTURE_BASE_FORMAT_PVRTII4BPP:
        return 4;
    case SCE_GXM_TEXTURE_BASE_FORMAT_UBC1:
        return 4;
    case SCE_GXM_TEXTURE_BASE_FORMAT_UBC2:
    case SCE_GXM_TEXTURE_BASE_FORMAT_UBC3:
        return 8;
    case SCE_GXM_TEXTURE_BASE_FORMAT_YUV420P2:
    case SCE_GXM_TEXTURE_BASE_FORMAT_YUV420P3:
    case SCE_GXM_TEXTURE_BASE_FORMAT_YUV422:
        return 16; // NOTE: I'm not sure this is right.
    case SCE_GXM_TEXTURE_BASE_FORMAT_P4:
        return 4;
    case SCE_GXM_TEXTURE_BASE_FORMAT_P8:
        return 8;
    case SCE_GXM_TEXTURE_BASE_FORMAT_U8U8U8:
    case SCE_GXM_TEXTURE_BASE_FORMAT_S8S8S8:
        return 24;
    case SCE_GXM_TEXTURE_BASE_FORMAT_U2F10F10F10:
        return 32;
    }

    return 0;
}

// Based on this: http://xen.firefly.nu/up/rearrange.c.html
// Thanks daniel from GXTConvert finding this out first

// Inverse of Part1By1 - "delete" all odd-indexed bits
static uint32_t compact_one_by_one(uint32_t x) {
    x &= 0x55555555; // x = -f-e -d-c -b-a -9-8 -7-6 -5-4 -3-2 -1-0
    x = (x ^ (x >> 1)) & 0x33333333; // x = --fe --dc --ba --98 --76 --54 --32 --10
    x = (x ^ (x >> 2)) & 0x0f0f0f0f; // x = ---- fedc ---- ba98 ---- 7654 ---- 3210
    x = (x ^ (x >> 4)) & 0x00ff00ff; // x = ---- ---- fedc ba98 ---- ---- 7654 3210
    x = (x ^ (x >> 8)) & 0x0000ffff; // x = ---- ---- ---- ---- fedc ba98 7654 3210
    return x;
}

static uint32_t decode_morton2_x(uint32_t code) {
    return compact_one_by_one(code >> 0);
}

static uint32_t decode_morton2_y(uint32_t code) {
    return compact_one_by_one(code >> 1);
}

void swizzled_texture_to_linear_texture(uint8_t *dest, const uint8_t *src, uint16_t width, uint16_t height, uint8_t bits_per_pixel) {
    if (bits_per_pixel % 8 != 0) {
        // Don't support yet
        return;
    }

    uint8_t bytes_per_pixel = (bits_per_pixel + 7) >> 3;

    for (uint32_t i = 0; i < static_cast<uint32_t>(width * height); i++) {
        size_t min = width < height ? width : height;
        size_t k = static_cast<size_t>(log2(min));

        size_t x, y;
        if (height < width) {
            // XXXyxyxyx → XXXxxxyyy
            size_t j = i >> (2 * k) << (2 * k)
                | (decode_morton2_y(i) & (min - 1)) << k
                | (decode_morton2_x(i) & (min - 1)) << 0;
            x = j / height;
            y = j % height;
        } else {
            // YYYyxyxyx → YYYyyyxxx
            size_t j = i >> (2 * k) << (2 * k)
                | (decode_morton2_x(i) & (min - 1)) << k
                | (decode_morton2_y(i) & (min - 1)) << 0;
            x = j % width;
            y = j / width;
        }

        std::memcpy(dest + (y * width + x) * bytes_per_pixel, src + i * bytes_per_pixel, bytes_per_pixel);
    }
}

void tiled_texture_to_linear_texture(uint8_t *dest, const uint8_t *src, uint16_t width, uint16_t height, uint8_t bits_per_pixel) {
    // 32x32 block is assembled to tiled.
    if (bits_per_pixel % 8 != 0) {
        // Don't support yet
        return;
    }

    const uint16_t width_in_tiles = (width + 31) >> 5;

    for (uint16_t y = 0; y < width; y++) {
        for (uint16_t x = 0; x < height; x++) {
            // Calculate texel address in title
            const uint16_t texel_offset_in_title = (x & 0b11111) | ((y & 0b11111) << 4);
            const uint16_t tile_address = (x & 0b111111100000) + width_in_tiles * (y & 0b111111100000);

            uint32_t offset = ((tile_address << 9) | (texel_offset_in_title)) * (bits_per_pixel >> 3);

            // Make scanline
            memcpy(dest + ((y * width) + x) * (bits_per_pixel >> 3), src + offset, bits_per_pixel >> 3);
        }
    }
}

} // namespace renderer::texture