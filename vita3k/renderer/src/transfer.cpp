// Vita3K emulator project
// Copyright (C) 2024 Vita3K team
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
#include <gxm/types.h>
#include <renderer/commands.h>
#include <renderer/driver_functions.h>
#include <renderer/functions.h>
#include <renderer/state.h>
#include <renderer/types.h>
#include <util/keywords.h>
#include <util/log.h>
#include <util/tracy.h>

namespace renderer {

template <typename T, SceGxmTransferColorKeyMode mode, SceGxmTransferType src_type, SceGxmTransferType dst_type>
static void perform_transfer_copy_impl(MemState &mem, const SceGxmTransferImage &src, const SceGxmTransferImage &dst, uint32_t key_value, uint32_t key_mask) {
    T *__restrict__ src_ptr = src.address.cast<T>().get(mem);
    T *__restrict__ dst_ptr = dst.address.cast<T>().get(mem);

    auto compute_offset = [&](uint32_t dx, uint32_t dy, const SceGxmTransferImage &img, SceGxmTransferType type) -> int32_t {
        const int32_t stride_pixel = img.stride / sizeof(T);
        if (type == SCE_GXM_TRANSFER_LINEAR) {
            return dy * stride_pixel + dx;
        } else if (type == SCE_GXM_TRANSFER_TILED) {
            // tiles are 32x32, you have the offset within the tile then the offset of the tile
            const uint32_t texel_offset_in_tile = ((dy % 32) * 32) + (dx % 32);
            const int32_t tile_address = (stride_pixel / 32) * (dy / 32) + (dx / 32);

            return tile_address * 1024 + texel_offset_in_tile;
        } else {
            return texture::encode_morton(dx, dy, img.width, img.height);
        }
    };

    for (uint32_t dx = 0; dx < src.width; dx++) {
        for (uint32_t dy = 0; dy < src.height; dy++) {
            // compute offset depending on the texture type used
            // the function compute_offset gets inlined
            uint32_t src_offset = compute_offset(src.x + dx, src.y + dy, src, src_type);
            uint32_t dst_offset = compute_offset(dst.x + dx, dst.y + dy, dst, dst_type);

            T value = src_ptr[src_offset];
            if constexpr (mode == SCE_GXM_TRANSFER_COLORKEY_PASS) {
                if ((value & key_mask) != key_value)
                    continue;
            } else if constexpr (mode == SCE_GXM_TRANSFER_COLORKEY_REJECT) {
                if ((value & key_mask) == key_value)
                    continue;
            }

            dst_ptr[dst_offset] = value;
        }
    }
}

template <typename T, SceGxmTransferColorKeyMode mode, SceGxmTransferType src_type>
static void perform_transfer_copy_dst_type(MemState &mem, const SceGxmTransferImage &src, const SceGxmTransferImage &dst, SceGxmTransferType dst_type, uint32_t key_value, uint32_t key_mask) {
    switch (dst_type) {
    case SCE_GXM_TRANSFER_LINEAR:
        perform_transfer_copy_impl<T, mode, src_type, SCE_GXM_TRANSFER_LINEAR>(mem, src, dst, key_value, key_mask);
        break;
    case SCE_GXM_TRANSFER_SWIZZLED:
        perform_transfer_copy_impl<T, mode, src_type, SCE_GXM_TRANSFER_SWIZZLED>(mem, src, dst, key_value, key_mask);
        break;
    case SCE_GXM_TRANSFER_TILED:
        perform_transfer_copy_impl<T, mode, src_type, SCE_GXM_TRANSFER_TILED>(mem, src, dst, key_value, key_mask);
        break;
    default:
        LOG_ERROR("Unknown transfer key mode {}", fmt::underlying(mode));
        break;
    }
}

template <typename T, SceGxmTransferColorKeyMode mode>
static void perform_transfer_copy_src_type(MemState &mem, const SceGxmTransferImage &src, const SceGxmTransferImage &dst, SceGxmTransferType src_type, SceGxmTransferType dst_type, uint32_t key_value, uint32_t key_mask) {
    switch (src_type) {
    case SCE_GXM_TRANSFER_LINEAR:
        perform_transfer_copy_dst_type<T, mode, SCE_GXM_TRANSFER_LINEAR>(mem, src, dst, dst_type, key_value, key_mask);
        break;
    case SCE_GXM_TRANSFER_SWIZZLED:
        perform_transfer_copy_dst_type<T, mode, SCE_GXM_TRANSFER_SWIZZLED>(mem, src, dst, dst_type, key_value, key_mask);
        break;
    case SCE_GXM_TRANSFER_TILED:
        perform_transfer_copy_dst_type<T, mode, SCE_GXM_TRANSFER_TILED>(mem, src, dst, dst_type, key_value, key_mask);
        break;
    default:
        LOG_ERROR("Unknown transfer key mode {}", fmt::underlying(mode));
        break;
    }
}

template <typename T>
static void perform_transfer_copy_mode(MemState &mem, const SceGxmTransferImage &src, const SceGxmTransferImage &dst, SceGxmTransferType src_type, SceGxmTransferType dst_type, uint32_t key_value, uint32_t key_mask, SceGxmTransferColorKeyMode mode) {
    switch (mode) {
    case SCE_GXM_TRANSFER_COLORKEY_NONE:
        perform_transfer_copy_src_type<T, SCE_GXM_TRANSFER_COLORKEY_NONE>(mem, src, dst, src_type, dst_type, key_value, key_mask);
        break;
    case SCE_GXM_TRANSFER_COLORKEY_PASS:
        perform_transfer_copy_src_type<T, SCE_GXM_TRANSFER_COLORKEY_PASS>(mem, src, dst, src_type, dst_type, key_value, key_mask);
        break;
    case SCE_GXM_TRANSFER_COLORKEY_REJECT:
        perform_transfer_copy_src_type<T, SCE_GXM_TRANSFER_COLORKEY_REJECT>(mem, src, dst, src_type, dst_type, key_value, key_mask);
        break;
    default:
        LOG_ERROR("Unknown transfer key mode {}", fmt::underlying(mode));
        break;
    }
}

COMMAND(handle_transfer_copy) {
    TRACY_FUNC_COMMANDS(handle_transfer_copy);
    const uint32_t colorKeyValue = helper.pop<uint32_t>();
    const uint32_t colorKeyMask = helper.pop<uint32_t>();
    SceGxmTransferColorKeyMode colorKeyMode = helper.pop<SceGxmTransferColorKeyMode>();
    const SceGxmTransferImage *images = helper.pop<SceGxmTransferImage *>();
    const SceGxmTransferImage &src = images[0];
    const SceGxmTransferImage &dst = images[1];
    SceGxmTransferType src_type = helper.pop<SceGxmTransferType>();
    SceGxmTransferType dst_type = helper.pop<SceGxmTransferType>();

    if (src.format != dst.format) {
        LOG_ERROR_ONCE("Unhandled format conversion from {} to {}", log_hex(fmt::underlying(src.format)), log_hex(fmt::underlying(dst.format)));
        delete[] images;
        return;
    }

    if (colorKeyMask != SCE_GXM_TRANSFER_COLORKEY_NONE && src.format != SCE_GXM_TRANSFER_FORMAT_U8U8U8U8_ABGR) {
        LOG_ERROR_ONCE("Transfer copy with non-zero key mask not handled for format {}", log_hex(fmt::underlying(src.format)));
    }

    // Get bits per pixel of the image
    const uint32_t bpp = gxm::get_bits_per_pixel(src.format);

    // use a specialized function for each type (more optimized)
    switch (bpp) {
    case 8:
        perform_transfer_copy_src_type<uint8_t, SCE_GXM_TRANSFER_COLORKEY_NONE>(mem, src, dst, src_type, dst_type, colorKeyValue, colorKeyMask);
        break;
    case 16:
        perform_transfer_copy_src_type<uint16_t, SCE_GXM_TRANSFER_COLORKEY_NONE>(mem, src, dst, src_type, dst_type, colorKeyValue, colorKeyMask);
        break;
    case 24:
        perform_transfer_copy_src_type<std::array<uint8_t, 3>, SCE_GXM_TRANSFER_COLORKEY_NONE>(mem, src, dst, src_type, dst_type, colorKeyValue, colorKeyMask);
        break;
    case 32:
        perform_transfer_copy_mode<uint32_t>(mem, src, dst, src_type, dst_type, colorKeyValue, colorKeyMask, colorKeyMode);
        break;
    case 64:
        perform_transfer_copy_src_type<uint64_t, SCE_GXM_TRANSFER_COLORKEY_NONE>(mem, src, dst, src_type, dst_type, colorKeyValue, colorKeyMask);
        break;
    case 128:
        perform_transfer_copy_src_type<std::array<uint64_t, 2>, SCE_GXM_TRANSFER_COLORKEY_NONE>(mem, src, dst, src_type, dst_type, colorKeyValue, colorKeyMask);
        break;
    }

    delete[] images;
}

COMMAND(handle_transfer_downscale) {
    TRACY_FUNC_COMMANDS(handle_transfer_downscale);
    const SceGxmTransferImage *src = helper.pop<SceGxmTransferImage *>();
    const SceGxmTransferImage *dest = helper.pop<SceGxmTransferImage *>();

    const auto src_bpp = gxm::get_bits_per_pixel(src->format);
    const auto dest_bpp = gxm::get_bits_per_pixel(dest->format);
    const uint32_t src_bytes_per_pixel = (src_bpp + 7) >> 3;
    const uint32_t dest_bytes_per_pixel = (dest_bpp + 7) >> 3;

    for (uint32_t y = 0; y < src->height; y += 2) {
        for (uint32_t x = 0; x < src->width; x += 2) {
            // Set offset of source and destination
            const auto src_offset = ((x + src->x) * src_bytes_per_pixel) + ((y + src->y) * src->stride);
            const auto dest_offset = (x / 2 + dest->x) * dest_bytes_per_pixel + (y / 2 + dest->y) * dest->stride;

            // Set pointer of source and destination
            const auto src_ptr = (uint8_t *)src->address.get(mem) + src_offset;
            auto dest_ptr = (uint8_t *)dest->address.get(mem) + dest_offset;

            // Copy result in destination
            memcpy(dest_ptr, src_ptr, dest_bytes_per_pixel);
        }
    }

    // TODO: handle case where dest is a cached surface

    delete src;
    delete dest;
}

COMMAND(handle_transfer_fill) {
    TRACY_FUNC_COMMANDS(handle_transfer_fill);
    const uint32_t fill_color = helper.pop<uint32_t>();
    const SceGxmTransferImage *dest = helper.pop<SceGxmTransferImage *>();

    const auto bpp = gxm::get_bits_per_pixel(dest->format);

    const uint32_t bytes_per_pixel = (bpp + 7) >> 3;
    for (uint32_t y = 0; y < dest->height; y++) {
        for (uint32_t x = 0; x < dest->width; x++) {
            // Set offset of destination
            const auto dest_offset = ((x + dest->x) * bytes_per_pixel) + ((y + dest->y) * dest->stride);

            // Set pointer of destination
            auto dest_ptr = (uint8_t *)dest->address.get(mem) + dest_offset;

            // Fill color in destination
            memcpy(dest_ptr, &fill_color, bytes_per_pixel);
        }
    }

    // TODO: handle case where dest is a cached surface

    delete dest;
}

} // namespace renderer
