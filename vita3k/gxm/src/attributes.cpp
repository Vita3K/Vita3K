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

#include <gxm/functions.h>
#include <util/log.h>

namespace gxm {
size_t attribute_format_size(SceGxmAttributeFormat format) {
    switch (format) {
    case SCE_GXM_ATTRIBUTE_FORMAT_U8:
    case SCE_GXM_ATTRIBUTE_FORMAT_U8N:
    case SCE_GXM_ATTRIBUTE_FORMAT_S8:
    case SCE_GXM_ATTRIBUTE_FORMAT_S8N:
        return 1;
    case SCE_GXM_ATTRIBUTE_FORMAT_U16:
    case SCE_GXM_ATTRIBUTE_FORMAT_U16N:
    case SCE_GXM_ATTRIBUTE_FORMAT_S16:
    case SCE_GXM_ATTRIBUTE_FORMAT_S16N:
    case SCE_GXM_ATTRIBUTE_FORMAT_F16:
        return 2;
    case SCE_GXM_ATTRIBUTE_FORMAT_F32:
        return 4;
    case SCE_GXM_ATTRIBUTE_FORMAT_UNTYPED:
        return 4;
    default:
        LOG_ERROR("Unsupported attribute format {}", log_hex(format));
        return 4;
    }
}

size_t index_element_size(SceGxmIndexFormat format) {
    return (format == SCE_GXM_INDEX_FORMAT_U16) ? 2 : 4;
}
} // namespace gxm