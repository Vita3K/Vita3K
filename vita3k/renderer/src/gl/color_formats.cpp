// Vita3K emulator project
// Copyright (C) 2025 Vita3K team
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

#include <glutil/gl.h>
#include <gxm/types.h>

#include <gxm/functions.h>
#include <util/log.h>

namespace renderer::color {
size_t bits_per_pixel(SceGxmColorBaseFormat base_format);
}

namespace renderer::gl::color {

static const GLint swizzle_abgr[4] = { GL_RED, GL_GREEN, GL_BLUE, GL_ALPHA };
static const GLint swizzle_argb[4] = { GL_BLUE, GL_GREEN, GL_RED, GL_ALPHA };
static const GLint swizzle_rgba[4] = { GL_ALPHA, GL_BLUE, GL_GREEN, GL_RED };
static const GLint swizzle_bgra[4] = { GL_GREEN, GL_BLUE, GL_ALPHA, GL_RED };

static const GLint swizzle_rgb[4] = { GL_BLUE, GL_GREEN, GL_RED, GL_ONE };
static const GLint swizzle_bgr[4] = { GL_RED, GL_GREEN, GL_BLUE, GL_ONE };

static const GLint swizzle_gr[4] = { GL_RED, GL_GREEN, GL_ZERO, GL_ONE };
static const GLint swizzle_rg[4] = { GL_GREEN, GL_RED, GL_ZERO, GL_ONE };
static const GLint swizzle_ar[4] = { GL_RED, GL_ZERO, GL_ZERO, GL_GREEN };
static const GLint swizzle_ra[4] = { GL_RED, GL_ZERO, GL_ZERO, GL_GREEN };

static const GLint swizzle_r[4] = { GL_RED, GL_ZERO, GL_ZERO, GL_ONE };

static const GLint *translate_swizzle(SceGxmColorSwizzle4Mode mode) {
    switch (mode) {
    case SCE_GXM_COLOR_SWIZZLE4_ABGR:
        return swizzle_abgr;
    case SCE_GXM_COLOR_SWIZZLE4_ARGB:
        return swizzle_argb;
    case SCE_GXM_COLOR_SWIZZLE4_RGBA:
        return swizzle_rgba;
    case SCE_GXM_COLOR_SWIZZLE4_BGRA:
        return swizzle_bgra;
    default:
        break;
    }

    return swizzle_abgr;
}

static const GLint *translate_swizzle(SceGxmColorSwizzle3Mode mode) {
    switch (mode) {
    case SCE_GXM_COLOR_SWIZZLE3_BGR:
        return swizzle_bgr;
    case SCE_GXM_COLOR_SWIZZLE3_RGB:
        return swizzle_rgb;
    }

    return swizzle_bgr;
}

static const GLint *translate_swizzle(SceGxmColorSwizzle2Mode mode) {
    switch (mode) {
    case SCE_GXM_COLOR_SWIZZLE2_GR:
        return swizzle_gr;
    case SCE_GXM_COLOR_SWIZZLE2_RG:
        return swizzle_rg;
    case SCE_GXM_COLOR_SWIZZLE2_RA:
        return swizzle_ra;
    case SCE_GXM_COLOR_SWIZZLE2_AR:
        return swizzle_ar;
    }

    return swizzle_gr;
}

static const GLint *translate_swizzle(SceGxmColorSwizzle1Mode mode) {
    switch (mode) {
    case SCE_GXM_COLOR_SWIZZLE1_R:
        return swizzle_r;
    case SCE_GXM_COLOR_SWIZZLE1_G:
        LOG_ERROR("unimplemented swizzle1 mode {}", log_hex(mode));
        break;
    }

    return swizzle_r;
}

// Translate popular color base format that can be bit-casted for purposes
GLenum translate_internal_format(SceGxmColorBaseFormat base_format) {
    switch (base_format) {
    case SCE_GXM_COLOR_BASE_FORMAT_U8U8U8:
        return GL_RGB8;

    case SCE_GXM_COLOR_BASE_FORMAT_U8U8U8U8:
        return GL_RGBA8;

    case SCE_GXM_COLOR_BASE_FORMAT_S8S8S8S8:
        return GL_RGBA8_SNORM;

    case SCE_GXM_COLOR_BASE_FORMAT_F16F16F16F16:
        return GL_RGBA16F;

    case SCE_GXM_COLOR_BASE_FORMAT_U2U10U10U10:
        return GL_RGB10_A2;

    case SCE_GXM_COLOR_BASE_FORMAT_F11F11F10:
        return GL_R11F_G11F_B10F;

    case SCE_GXM_COLOR_BASE_FORMAT_F32F32:
        return GL_RG32F;

    case SCE_GXM_COLOR_BASE_FORMAT_F32:
        return GL_R32F;

    case SCE_GXM_COLOR_BASE_FORMAT_F16:
        return GL_R16F;

    case SCE_GXM_COLOR_BASE_FORMAT_U8:
        return GL_R8;

    case SCE_GXM_COLOR_BASE_FORMAT_U8U8:
        return GL_RG8;

    case SCE_GXM_COLOR_BASE_FORMAT_U2F10F10F10:
        return GL_RGB10_A2;

    default:
        LOG_ERROR("Unknown base format {}", log_hex(base_format));
        return GL_RGBA8;
    }
}

GLenum translate_format(SceGxmColorBaseFormat base_format) {
    switch (base_format) {
    case SCE_GXM_COLOR_BASE_FORMAT_U8U8U8U8:
        return GL_RGBA;

    case SCE_GXM_COLOR_BASE_FORMAT_S8S8S8S8:
        return GL_RGBA;

    case SCE_GXM_COLOR_BASE_FORMAT_U2U10U10U10:
        return GL_RGBA;

    case SCE_GXM_COLOR_BASE_FORMAT_U8U8:
    case SCE_GXM_COLOR_BASE_FORMAT_F32F32:
        return GL_RG;

    case SCE_GXM_COLOR_BASE_FORMAT_F11F11F10:
    case SCE_GXM_COLOR_BASE_FORMAT_U8U8U8:
        return GL_RGB;

    case SCE_GXM_COLOR_BASE_FORMAT_F32:
    case SCE_GXM_COLOR_BASE_FORMAT_F16:
    case SCE_GXM_COLOR_BASE_FORMAT_U8:
        return GL_RED;

    default: return GL_RGBA;
    }
}

GLenum translate_type(SceGxmColorBaseFormat base_format) {
    switch (base_format) {
    case SCE_GXM_COLOR_BASE_FORMAT_U8:
    case SCE_GXM_COLOR_BASE_FORMAT_U8U8:
    case SCE_GXM_COLOR_BASE_FORMAT_U8U8U8:
    case SCE_GXM_COLOR_BASE_FORMAT_U8U8U8U8:
        return GL_UNSIGNED_BYTE;

    case SCE_GXM_COLOR_BASE_FORMAT_S8S8S8S8:
        return GL_BYTE;

    case SCE_GXM_COLOR_BASE_FORMAT_F16F16F16F16:
        return GL_HALF_FLOAT;

    case SCE_GXM_COLOR_BASE_FORMAT_U2U10U10U10:
        return GL_UNSIGNED_INT_2_10_10_10_REV;

    case SCE_GXM_COLOR_BASE_FORMAT_F11F11F10:
        return GL_UNSIGNED_INT_10F_11F_11F_REV;

    case SCE_GXM_COLOR_BASE_FORMAT_F32:
    case SCE_GXM_COLOR_BASE_FORMAT_F32F32:
        return GL_FLOAT;

    case SCE_GXM_COLOR_BASE_FORMAT_F16:
        return GL_HALF_FLOAT;

    default:
        return GL_UNSIGNED_BYTE;
    }
}

const GLint *translate_swizzle(SceGxmColorFormat fmt) {
    const SceGxmColorBaseFormat base_format = gxm::get_base_format(fmt);
    const uint32_t swizzle = fmt & SCE_GXM_COLOR_SWIZZLE_MASK;
    switch (base_format) {
    // 1 Component.
    case SCE_GXM_COLOR_BASE_FORMAT_U8U8U8U8:
    case SCE_GXM_COLOR_BASE_FORMAT_S8S8S8S8:
    case SCE_GXM_COLOR_BASE_FORMAT_F16F16F16F16:
    case SCE_GXM_COLOR_BASE_FORMAT_U2U10U10U10:
    case SCE_GXM_COLOR_BASE_FORMAT_U2F10F10F10:
        return translate_swizzle(static_cast<SceGxmColorSwizzle4Mode>(swizzle));

    case SCE_GXM_COLOR_BASE_FORMAT_SE5M9M9M9:
    case SCE_GXM_COLOR_BASE_FORMAT_U5U6U5:
    case SCE_GXM_COLOR_BASE_FORMAT_U8U8U8:
    case SCE_GXM_COLOR_BASE_FORMAT_F11F11F10:
        return translate_swizzle(static_cast<SceGxmColorSwizzle3Mode>(swizzle));

    case SCE_GXM_COLOR_BASE_FORMAT_F32F32:
    case SCE_GXM_COLOR_BASE_FORMAT_U8U8:
        return translate_swizzle(static_cast<SceGxmColorSwizzle2Mode>(swizzle));

    case SCE_GXM_COLOR_BASE_FORMAT_U8:
    case SCE_GXM_COLOR_BASE_FORMAT_F16:
    case SCE_GXM_COLOR_BASE_FORMAT_F32:
        return translate_swizzle(static_cast<SceGxmColorSwizzle1Mode>(swizzle));

    default:
        break;
    }

    return swizzle_abgr;
}

size_t bytes_per_pixel(SceGxmColorBaseFormat base_format) {
    return gxm::bits_per_pixel(base_format) >> 3;
}

size_t bytes_per_pixel_in_gl_storage(SceGxmColorBaseFormat base_format) {
    switch (base_format) {
    case SCE_GXM_COLOR_BASE_FORMAT_U8:
        return 1;
    case SCE_GXM_COLOR_BASE_FORMAT_F16:
    case SCE_GXM_COLOR_BASE_FORMAT_U8U8:
        return 2;
    case SCE_GXM_COLOR_BASE_FORMAT_U8U8U8:
        return 3;
    case SCE_GXM_COLOR_BASE_FORMAT_U8U8U8U8:
    case SCE_GXM_COLOR_BASE_FORMAT_S8S8S8S8:
    case SCE_GXM_COLOR_BASE_FORMAT_U2U10U10U10:
    case SCE_GXM_COLOR_BASE_FORMAT_F32:
        return 4;
    case SCE_GXM_COLOR_BASE_FORMAT_F16F16F16F16:
    case SCE_GXM_COLOR_BASE_FORMAT_F32F32:
        return 8;
    default:
        break;
    }

    return 4;
}

bool is_write_surface_stored_rawly(SceGxmColorBaseFormat base_format) {
    return (base_format == SCE_GXM_COLOR_BASE_FORMAT_F16F16F16F16);
}

bool is_write_surface_non_linearity_filtering(SceGxmColorBaseFormat base_format) {
    return ((base_format == SCE_GXM_COLOR_BASE_FORMAT_F32) || (base_format == SCE_GXM_COLOR_BASE_FORMAT_F32F32));
}

GLenum get_raw_store_internal_type(SceGxmColorBaseFormat base_format) {
    switch (base_format) {
    case SCE_GXM_COLOR_BASE_FORMAT_F16F16F16F16:
        return GL_RGBA16UI;

    default:
        break;
    }

    return GL_RGBA16UI;
}

GLenum get_raw_store_upload_format_type(SceGxmColorBaseFormat base_format) {
    switch (base_format) {
    case SCE_GXM_COLOR_BASE_FORMAT_F16F16F16F16:
        return GL_RGBA_INTEGER;

    default:
        break;
    }

    return GL_RGBA_INTEGER;
}

GLenum get_raw_store_upload_data_type(SceGxmColorBaseFormat base_format) {
    switch (base_format) {
    case SCE_GXM_COLOR_BASE_FORMAT_F16F16F16F16:
        return GL_UNSIGNED_SHORT;

    default:
        break;
    }

    return GL_RGBA_INTEGER;
}
} // namespace renderer::gl::color
