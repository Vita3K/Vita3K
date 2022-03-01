// Vita3K emulator project
// Copyright (C) 2022 Vita3K team
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

#include <gxm/types.h>

namespace gxm {
uint32_t get_bits_per_pixel(SceGxmTransferFormat Format) {
    switch (Format) {
    case SCE_GXM_TRANSFER_FORMAT_U8_R:
        return 8;
    case SCE_GXM_TRANSFER_FORMAT_U4U4U4U4_ABGR:
    case SCE_GXM_TRANSFER_FORMAT_U1U5U5U5_ABGR:
    case SCE_GXM_TRANSFER_FORMAT_U5U6U5_BGR:
    case SCE_GXM_TRANSFER_FORMAT_U8U8_GR:
    case SCE_GXM_TRANSFER_FORMAT_VYUY422:
    case SCE_GXM_TRANSFER_FORMAT_YVYU422:
    case SCE_GXM_TRANSFER_FORMAT_UYVY422:
    case SCE_GXM_TRANSFER_FORMAT_YUYV422:
    case SCE_GXM_TRANSFER_FORMAT_RAW16:
        return 16;
    case SCE_GXM_TRANSFER_FORMAT_U8U8U8_BGR:
        return 24;
    case SCE_GXM_TRANSFER_FORMAT_U8U8U8U8_ABGR:
    case SCE_GXM_TRANSFER_FORMAT_U2U10U10U10_ABGR:
    case SCE_GXM_TRANSFER_FORMAT_RAW32:
        return 32;
    case SCE_GXM_TRANSFER_FORMAT_RAW64:
        return 64;
    case SCE_GXM_TRANSFER_FORMAT_RAW128:
        return 128;
    }

    return 0;
}
} // namespace gxm
