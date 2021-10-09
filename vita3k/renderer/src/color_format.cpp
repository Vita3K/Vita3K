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

#include <gxm/types.h>

namespace renderer::color {

size_t bits_per_pixel(SceGxmColorBaseFormat base_format) {
    switch (base_format) {
    case SCE_GXM_COLOR_BASE_FORMAT_U8:
    case SCE_GXM_COLOR_BASE_FORMAT_S8:
        return 8;
    case SCE_GXM_COLOR_BASE_FORMAT_U5U6U5:
    case SCE_GXM_COLOR_BASE_FORMAT_U1U5U5U5:
    case SCE_GXM_COLOR_BASE_FORMAT_U4U4U4U4:
    case SCE_GXM_COLOR_BASE_FORMAT_U8U3U3U2:
    case SCE_GXM_COLOR_BASE_FORMAT_F16:
    case SCE_GXM_COLOR_BASE_FORMAT_S16:
    case SCE_GXM_COLOR_BASE_FORMAT_U16:
    case SCE_GXM_COLOR_BASE_FORMAT_S5S5U6:
    case SCE_GXM_COLOR_BASE_FORMAT_U8U8:
    case SCE_GXM_COLOR_BASE_FORMAT_S8S8:
        return 16;
    case SCE_GXM_COLOR_BASE_FORMAT_U8U8U8:
        return 24;
    case SCE_GXM_COLOR_BASE_FORMAT_U8U8U8U8:
    case SCE_GXM_COLOR_BASE_FORMAT_F16F16:
    case SCE_GXM_COLOR_BASE_FORMAT_F32:
    case SCE_GXM_COLOR_BASE_FORMAT_S16S16:
    case SCE_GXM_COLOR_BASE_FORMAT_U16U16:
    case SCE_GXM_COLOR_BASE_FORMAT_U2U10U10U10:
    case SCE_GXM_COLOR_BASE_FORMAT_U8S8S8U8:
    case SCE_GXM_COLOR_BASE_FORMAT_S8S8S8S8:
    case SCE_GXM_COLOR_BASE_FORMAT_F11F11F10:
    case SCE_GXM_COLOR_BASE_FORMAT_SE5M9M9M9:
    case SCE_GXM_COLOR_BASE_FORMAT_U2F10F10F10:
        return 32;
    case SCE_GXM_COLOR_BASE_FORMAT_F16F16F16F16:
    case SCE_GXM_COLOR_BASE_FORMAT_F32F32:
        return 64;
    }

    return 0;
}

} // namespace renderer::color
