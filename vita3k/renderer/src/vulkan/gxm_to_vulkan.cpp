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

#include <renderer/vulkan/gxm_to_vulkan.h>

#include <gxm/functions.h>
#include <util/log.h>

namespace renderer::vulkan {

vk::Format translate_attribute_format(SceGxmAttributeFormat format, unsigned int component_count, bool is_integer, bool is_signed) {
    if (component_count == 0 || component_count > 4 || format > SCE_GXM_ATTRIBUTE_FORMAT_UNTYPED)
        LOG_ERROR("Unsupported attribute format {}x{}", log_hex(format), component_count);

    // Ahhhhh, I don't want to see this function ever again
    static constexpr vk::Format formats_integer[][4] = {
        /*SCE_GXM_ATTRIBUTE_FORMAT_U8*/ { vk::Format::eR8Uint, vk::Format::eR8G8Uint, vk::Format::eR8G8B8Uint, vk::Format::eR8G8B8A8Uint },
        /*SCE_GXM_ATTRIBUTE_FORMAT_S8*/ { vk::Format::eR8Sint, vk::Format::eR8G8Sint, vk::Format::eR8G8B8Sint, vk::Format::eR8G8B8A8Sint },
        /*SCE_GXM_ATTRIBUTE_FORMAT_U16*/ { vk::Format::eR16Uint, vk::Format::eR16G16Uint, vk::Format::eR16G16B16Uint, vk::Format::eR16G16B16A16Uint },
        /*SCE_GXM_ATTRIBUTE_FORMAT_S16*/ { vk::Format::eR16Sint, vk::Format::eR16G16Sint, vk::Format::eR16G16B16Sint, vk::Format::eR16G16B16A16Sint },
    };

    static constexpr vk::Format formats_integer_as_float[][4] = {
        /*SCE_GXM_ATTRIBUTE_FORMAT_U8*/ { vk::Format::eR8Uscaled, vk::Format::eR8G8Uscaled, vk::Format::eR8G8B8Uscaled, vk::Format::eR8G8B8A8Uscaled },
        /*SCE_GXM_ATTRIBUTE_FORMAT_S8*/ { vk::Format::eR8Sscaled, vk::Format::eR8G8Sscaled, vk::Format::eR8G8B8Sscaled, vk::Format::eR8G8B8A8Sscaled },
        /*SCE_GXM_ATTRIBUTE_FORMAT_U16*/ { vk::Format::eR16Uscaled, vk::Format::eR16G16Uscaled, vk::Format::eR16G16B16Uscaled, vk::Format::eR16G16B16A16Uscaled },
        /*SCE_GXM_ATTRIBUTE_FORMAT_S16*/ { vk::Format::eR16Sscaled, vk::Format::eR16G16Sscaled, vk::Format::eR16G16B16Sscaled, vk::Format::eR16G16B16A16Sscaled },
    };

    static constexpr vk::Format formats_float[][4] = {
        /*SCE_GXM_ATTRIBUTE_FORMAT_U8N*/ { vk::Format::eR8Unorm, vk::Format::eR8G8Unorm, vk::Format::eR8G8B8Unorm, vk::Format::eR8G8B8A8Unorm },
        /*SCE_GXM_ATTRIBUTE_FORMAT_S8N*/ { vk::Format::eR8Snorm, vk::Format::eR8G8Snorm, vk::Format::eR8G8B8Snorm, vk::Format::eR8G8B8A8Snorm },
        /*SCE_GXM_ATTRIBUTE_FORMAT_U16N*/ { vk::Format::eR16Unorm, vk::Format::eR16G16Unorm, vk::Format::eR16G16B16Unorm, vk::Format::eR16G16B16A16Unorm },
        /*SCE_GXM_ATTRIBUTE_FORMAT_S16N*/ { vk::Format::eR16Snorm, vk::Format::eR16G16Snorm, vk::Format::eR16G16B16Snorm, vk::Format::eR16G16B16A16Snorm },
        /*SCE_GXM_ATTRIBUTE_FORMAT_F16*/ { vk::Format::eR16Sfloat, vk::Format::eR16G16Sfloat, vk::Format::eR16G16B16Sfloat, vk::Format::eR16G16B16A16Sfloat },
        /*SCE_GXM_ATTRIBUTE_FORMAT_F32*/ { vk::Format::eR32Sfloat, vk::Format::eR32G32Sfloat, vk::Format::eR32G32B32Sfloat, vk::Format::eR32G32B32A32Sfloat },
    };

    static constexpr vk::Format formats_untyped[][4] = {
        /*SCE_GXM_ATTRIBUTE_FORMAT_UNTYPED unsigned*/ { vk::Format::eR32Uint, vk::Format::eR32G32Uint, vk::Format::eR32G32B32Uint, vk::Format::eR32G32B32A32Uint },
        /*SCE_GXM_ATTRIBUTE_FORMAT_UNTYPED signed*/ { vk::Format::eR32Sint, vk::Format::eR32G32Sint, vk::Format::eR32G32B32Sint, vk::Format::eR32G32B32A32Sint },
    };

    const int format_idx = format;
    if (format_idx < SCE_GXM_ATTRIBUTE_FORMAT_U8N) {
        if (is_integer)
            return formats_integer[format_idx][component_count - 1];
        else
            return formats_integer_as_float[format_idx][component_count - 1];
    } else if (format == SCE_GXM_ATTRIBUTE_FORMAT_UNTYPED) {
        return formats_untyped[is_signed][component_count - 1];
    } else {
        return formats_float[format_idx - SCE_GXM_ATTRIBUTE_FORMAT_U8N][component_count - 1];
    }
}

vk::BlendFactor translate_blend_factor(const SceGxmBlendFactor blend_factor) {
    switch (blend_factor) {
    case SCE_GXM_BLEND_FACTOR_ZERO:
        return vk::BlendFactor::eZero;
    case SCE_GXM_BLEND_FACTOR_ONE:
        return vk::BlendFactor::eOne;
    case SCE_GXM_BLEND_FACTOR_SRC_COLOR:
        return vk::BlendFactor::eSrcColor;
    case SCE_GXM_BLEND_FACTOR_ONE_MINUS_SRC_COLOR:
        return vk::BlendFactor::eOneMinusSrcColor;
    case SCE_GXM_BLEND_FACTOR_SRC_ALPHA:
        return vk::BlendFactor::eSrcAlpha;
    case SCE_GXM_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA:
        return vk::BlendFactor::eOneMinusSrcAlpha;
    case SCE_GXM_BLEND_FACTOR_DST_COLOR:
        return vk::BlendFactor::eDstColor;
    case SCE_GXM_BLEND_FACTOR_ONE_MINUS_DST_COLOR:
        return vk::BlendFactor::eOneMinusDstColor;
    case SCE_GXM_BLEND_FACTOR_DST_ALPHA:
        return vk::BlendFactor::eDstAlpha;
    case SCE_GXM_BLEND_FACTOR_ONE_MINUS_DST_ALPHA:
        return vk::BlendFactor::eOneMinusDstAlpha;
    case SCE_GXM_BLEND_FACTOR_SRC_ALPHA_SATURATE:
        return vk::BlendFactor::eSrcAlphaSaturate;
    case SCE_GXM_BLEND_FACTOR_DST_ALPHA_SATURATE:
        return vk::BlendFactor::eDstAlpha; // TODO Not supported.
    default:
        LOG_ERROR("Unknown blend factor: {}", fmt::underlying(blend_factor));
        return vk::BlendFactor::eOne;
    }
}

vk::BlendOp translate_blend_func(const SceGxmBlendFunc blend_func) {
    switch (blend_func) {
    case SCE_GXM_BLEND_FUNC_NONE:
    case SCE_GXM_BLEND_FUNC_ADD:
        return vk::BlendOp::eAdd;
    case SCE_GXM_BLEND_FUNC_SUBTRACT:
        return vk::BlendOp::eSubtract;
    case SCE_GXM_BLEND_FUNC_REVERSE_SUBTRACT:
        return vk::BlendOp::eReverseSubtract;
    case SCE_GXM_BLEND_FUNC_MIN:
        return vk::BlendOp::eMin;
    case SCE_GXM_BLEND_FUNC_MAX:
        return vk::BlendOp::eMax;
    default:
        LOG_ERROR("Unknown blend func: {}", fmt::underlying(blend_func));
        return vk::BlendOp::eAdd;
    }
}

vk::PrimitiveTopology translate_primitive(SceGxmPrimitiveType primitive) {
    switch (primitive) {
    case SCE_GXM_PRIMITIVE_TRIANGLES:
        return vk::PrimitiveTopology::eTriangleList;
    case SCE_GXM_PRIMITIVE_TRIANGLE_STRIP:
        return vk::PrimitiveTopology::eTriangleStrip;
    case SCE_GXM_PRIMITIVE_TRIANGLE_FAN:
        return vk::PrimitiveTopology::eTriangleFan;
    case SCE_GXM_PRIMITIVE_LINES:
        return vk::PrimitiveTopology::eLineList;
    case SCE_GXM_PRIMITIVE_POINTS:
        return vk::PrimitiveTopology::ePointList;
    case SCE_GXM_PRIMITIVE_TRIANGLE_EDGES: // Todo: Implement this
        LOG_WARN("Unsupported primitive: SCE_GXM_PRIMITIVE_TRIANGLE_EDGES, using SCE_GXM_PRIMITIVE_TRIANGLES instead");
        return vk::PrimitiveTopology::eTriangleList;
    default:
        LOG_ERROR("Unknown primitive: {}", fmt::underlying(primitive));
        return vk::PrimitiveTopology::eTriangleList;
    }
}

vk::CompareOp translate_depth_func(SceGxmDepthFunc depth_func) {
    switch (depth_func) {
    case SCE_GXM_DEPTH_FUNC_NEVER:
        return vk::CompareOp::eNever;
    case SCE_GXM_DEPTH_FUNC_LESS:
        return vk::CompareOp::eLess;
    case SCE_GXM_DEPTH_FUNC_EQUAL:
        return vk::CompareOp::eEqual;
    case SCE_GXM_DEPTH_FUNC_LESS_EQUAL:
        return vk::CompareOp::eLessOrEqual;
    case SCE_GXM_DEPTH_FUNC_GREATER:
        return vk::CompareOp::eGreater;
    case SCE_GXM_DEPTH_FUNC_NOT_EQUAL:
        return vk::CompareOp::eNotEqual;
    case SCE_GXM_DEPTH_FUNC_GREATER_EQUAL:
        return vk::CompareOp::eGreaterOrEqual;
    case SCE_GXM_DEPTH_FUNC_ALWAYS:
        return vk::CompareOp::eAlways;
    default:
        LOG_ERROR("Unknown depth func {}", log_hex(depth_func));
        return vk::CompareOp::eAlways;
    }
}

vk::PolygonMode translate_polygon_mode(SceGxmPolygonMode polygon_mode) {
    switch (polygon_mode) {
    case SCE_GXM_POLYGON_MODE_TRIANGLE_FILL:
        return vk::PolygonMode::eFill;
    case SCE_GXM_POLYGON_MODE_LINE:
    case SCE_GXM_POLYGON_MODE_TRIANGLE_LINE:
        return vk::PolygonMode::eLine;
    case SCE_GXM_POLYGON_MODE_POINT_10UV:
    case SCE_GXM_POLYGON_MODE_POINT:
    case SCE_GXM_POLYGON_MODE_POINT_01UV:
    case SCE_GXM_POLYGON_MODE_TRIANGLE_POINT:
        return vk::PolygonMode::ePoint;
    default:
        LOG_ERROR("Unknown polygon mode {}", log_hex(polygon_mode));
        return vk::PolygonMode::eFill;
    }
}
vk::CullModeFlags translate_cull_mode(SceGxmCullMode cull_mode) {
    // In gxm, the font face is always counter clockwise
    switch (cull_mode) {
    case SCE_GXM_CULL_NONE:
        return vk::CullModeFlagBits::eNone;
    case SCE_GXM_CULL_CW:
        return vk::CullModeFlagBits::eBack;
    case SCE_GXM_CULL_CCW:
        return vk::CullModeFlagBits::eFront;
    default:
        LOG_ERROR("Unknown cull mode {}", log_hex(cull_mode));
        return vk::CullModeFlagBits::eNone;
    }
}

vk::CompareOp translate_stencil_func(SceGxmStencilFunc stencil_func) {
    switch (stencil_func) {
    case SCE_GXM_STENCIL_FUNC_NEVER:
        return vk::CompareOp::eNever;
    case SCE_GXM_STENCIL_FUNC_LESS:
        return vk::CompareOp::eLess;
    case SCE_GXM_STENCIL_FUNC_EQUAL:
        return vk::CompareOp::eEqual;
    case SCE_GXM_STENCIL_FUNC_LESS_EQUAL:
        return vk::CompareOp::eLessOrEqual;
    case SCE_GXM_STENCIL_FUNC_GREATER:
        return vk::CompareOp::eGreater;
    case SCE_GXM_STENCIL_FUNC_NOT_EQUAL:
        return vk::CompareOp::eNotEqual;
    case SCE_GXM_STENCIL_FUNC_GREATER_EQUAL:
        return vk::CompareOp::eGreaterOrEqual;
    case SCE_GXM_STENCIL_FUNC_ALWAYS:
        return vk::CompareOp::eAlways;
    default:
        LOG_ERROR("Unknown stencil func {}", log_hex(stencil_func));
        return vk::CompareOp::eAlways;
    }
}

vk::StencilOp translate_stencil_op(SceGxmStencilOp stencil_op) {
    switch (stencil_op) {
    case SCE_GXM_STENCIL_OP_KEEP:
        return vk::StencilOp::eKeep;
    case SCE_GXM_STENCIL_OP_ZERO:
        return vk::StencilOp::eZero;
    case SCE_GXM_STENCIL_OP_REPLACE:
        return vk::StencilOp::eReplace;
    case SCE_GXM_STENCIL_OP_INCR:
        return vk::StencilOp::eIncrementAndClamp;
    case SCE_GXM_STENCIL_OP_DECR:
        return vk::StencilOp::eDecrementAndClamp;
    case SCE_GXM_STENCIL_OP_INVERT:
        return vk::StencilOp::eInvert;
    case SCE_GXM_STENCIL_OP_INCR_WRAP:
        return vk::StencilOp::eIncrementAndWrap;
    case SCE_GXM_STENCIL_OP_DECR_WRAP:
        return vk::StencilOp::eDecrementAndWrap;
    default:
        LOG_ERROR("Unknown stencil op {}", log_hex(stencil_op));
        return vk::StencilOp::eKeep;
    }
}

using Swizzle = vk::ComponentSwizzle;

static constexpr vk::ComponentMapping swizzle_identity = { Swizzle::eIdentity, Swizzle::eIdentity, Swizzle::eIdentity, Swizzle::eIdentity };

// SceGxmSwizzle1Mode
static constexpr vk::ComponentMapping swizzle_r001 = { Swizzle::eR, Swizzle::eZero, Swizzle::eZero, Swizzle::eOne };
static constexpr vk::ComponentMapping swizzle_r000 = { Swizzle::eR, Swizzle::eZero, Swizzle::eZero, Swizzle::eZero };
static constexpr vk::ComponentMapping swizzle_r111 = { Swizzle::eR, Swizzle::eOne, Swizzle::eOne, Swizzle::eOne };
static constexpr vk::ComponentMapping swizzle_rrrr = { Swizzle::eR, Swizzle::eR, Swizzle::eR, Swizzle::eR };
static constexpr vk::ComponentMapping swizzle_rrr0 = { Swizzle::eR, Swizzle::eR, Swizzle::eR, Swizzle::eZero };
static constexpr vk::ComponentMapping swizzle_rrr1 = { Swizzle::eR, Swizzle::eR, Swizzle::eR, Swizzle::eOne };
static constexpr vk::ComponentMapping swizzle_000r = { Swizzle::eZero, Swizzle::eZero, Swizzle::eZero, Swizzle::eR };
static constexpr vk::ComponentMapping swizzle_111r = { Swizzle::eOne, Swizzle::eOne, Swizzle::eOne, Swizzle::eR };
static constexpr vk::ComponentMapping swizzle_000a = { Swizzle::eZero, Swizzle::eZero, Swizzle::eZero, Swizzle::eR };

// SceGxmSwizzle2Mode
static constexpr vk::ComponentMapping swizzle_rg01 = { Swizzle::eR, Swizzle::eG, Swizzle::eZero, Swizzle::eOne };
static constexpr vk::ComponentMapping swizzle_gr01 = { Swizzle::eG, Swizzle::eR, Swizzle::eZero, Swizzle::eOne };
static constexpr vk::ComponentMapping swizzle_a00r = { Swizzle::eA, Swizzle::eZero, Swizzle::eZero, Swizzle::eR };
static constexpr vk::ComponentMapping swizzle_r00a = { Swizzle::eR, Swizzle::eZero, Swizzle::eZero, Swizzle::eA };
static constexpr vk::ComponentMapping swizzle_rg00 = { Swizzle::eR, Swizzle::eG, Swizzle::eZero, Swizzle::eZero };
static constexpr vk::ComponentMapping swizzle_rrrg = { Swizzle::eR, Swizzle::eR, Swizzle::eR, Swizzle::eG };
static constexpr vk::ComponentMapping swizzle_gggr = { Swizzle::eG, Swizzle::eG, Swizzle::eG, Swizzle::eR };
static constexpr vk::ComponentMapping swizzle_rgrg = { Swizzle::eR, Swizzle::eG, Swizzle::eR, Swizzle::eG };
static constexpr vk::ComponentMapping swizzle_gr00 = { Swizzle::eG, Swizzle::eR, Swizzle::eZero, Swizzle::eZero };

// SceGxmSwizzle3Mode
static constexpr vk::ComponentMapping swizzle_bgr1 = { Swizzle::eB, Swizzle::eG, Swizzle::eR, Swizzle::eOne };
static constexpr vk::ComponentMapping swizzle_rgb1 = { Swizzle::eR, Swizzle::eG, Swizzle::eB, Swizzle::eOne };

// SceGxmSwizzle4Mode
static constexpr vk::ComponentMapping swizzle_rgba = { Swizzle::eR, Swizzle::eG, Swizzle::eB, Swizzle::eA };
static constexpr vk::ComponentMapping swizzle_bgra = { Swizzle::eB, Swizzle::eG, Swizzle::eR, Swizzle::eA };
static constexpr vk::ComponentMapping swizzle_abgr = { Swizzle::eA, Swizzle::eB, Swizzle::eG, Swizzle::eR };
static constexpr vk::ComponentMapping swizzle_gbar = { Swizzle::eG, Swizzle::eB, Swizzle::eA, Swizzle::eR };
static constexpr vk::ComponentMapping swizzle_abg1 = { Swizzle::eA, Swizzle::eB, Swizzle::eG, Swizzle::eOne };
static constexpr vk::ComponentMapping swizzle_gba1 = { Swizzle::eG, Swizzle::eB, Swizzle::eA, Swizzle::eOne };

namespace color {

static vk::ComponentMapping translate_swizzle1(SceGxmColorSwizzle1Mode mode) {
    switch (mode) {
    case SCE_GXM_COLOR_SWIZZLE1_R:
        return swizzle_r001;
    case SCE_GXM_COLOR_SWIZZLE1_A:
        return swizzle_000a;
    default:
        LOG_ERROR("Unknown swizzle mode {}", log_hex(mode));
        return swizzle_identity;
    }
}

static vk::ComponentMapping translate_swizzle2(SceGxmColorSwizzle2Mode mode) {
    switch (mode) {
    case SCE_GXM_COLOR_SWIZZLE2_GR:
        return swizzle_rg01;
    case SCE_GXM_COLOR_SWIZZLE2_RG:
        return swizzle_gr01;
    default:
        LOG_ERROR("Unknown swizzle mode {}", log_hex(mode));
        return swizzle_identity;
    }
}

static vk::ComponentMapping translate_swizzle3(SceGxmColorSwizzle3Mode mode) {
    switch (mode) {
    case SCE_GXM_COLOR_SWIZZLE3_BGR:
        return swizzle_rgb1;
    case SCE_GXM_COLOR_SWIZZLE3_RGB:
        return swizzle_bgr1;
    default:
        LOG_ERROR("Unknown swizzle mode {}", log_hex(mode));
        return swizzle_identity;
    }
}

// Convert the swizzle when the vulkan format has a BGR (or RGB packed) layout
static vk::ComponentMapping translate_swizzle3_bgr(SceGxmColorSwizzle3Mode mode) {
    switch (mode) {
    case SCE_GXM_COLOR_SWIZZLE3_BGR:
        return swizzle_bgr1;
    case SCE_GXM_COLOR_SWIZZLE3_RGB:
        return swizzle_rgb1;
    default:
        LOG_ERROR("Unknown swizzle mode {}", log_hex(mode));
        return swizzle_identity;
    }
}

static vk::ComponentMapping translate_swizzle4(SceGxmColorSwizzle4Mode mode) {
    switch (mode) {
    case SCE_GXM_COLOR_SWIZZLE4_ABGR:
        return swizzle_rgba;
    case SCE_GXM_COLOR_SWIZZLE4_ARGB:
        return swizzle_bgra;
    case SCE_GXM_COLOR_SWIZZLE4_RGBA:
        return swizzle_abgr;
    case SCE_GXM_COLOR_SWIZZLE4_BGRA:
        return swizzle_gbar;
    default:
        LOG_ERROR("Unknown swizzle mode {}", log_hex(mode));
        return swizzle_identity;
    }
}

// Convert the swizzle when the vulkan format has a ABGR (or RGBA packed) layout
static vk::ComponentMapping translate_swizzle4_abgr(SceGxmColorSwizzle4Mode mode) {
    switch (mode) {
    case SCE_GXM_COLOR_SWIZZLE4_ABGR:
        return swizzle_abgr;
    case SCE_GXM_COLOR_SWIZZLE4_ARGB:
        return swizzle_gbar;
    case SCE_GXM_COLOR_SWIZZLE4_RGBA:
        return swizzle_rgba;
    case SCE_GXM_COLOR_SWIZZLE4_BGRA:
        return swizzle_bgra;
    default:
        LOG_ERROR("Unknown swizzle mode {}", log_hex(mode));
        return {};
    }
}

vk::ComponentMapping translate_swizzle(SceGxmColorFormat format) {
    const SceGxmColorBaseFormat base_format = gxm::get_base_format(format);
    const uint32_t swizzle = format & SCE_GXM_COLOR_SWIZZLE_MASK;
    switch (base_format) {
    case SCE_GXM_COLOR_BASE_FORMAT_U8:
    case SCE_GXM_COLOR_BASE_FORMAT_S8:
    case SCE_GXM_COLOR_BASE_FORMAT_U16:
    case SCE_GXM_COLOR_BASE_FORMAT_S16:
    case SCE_GXM_COLOR_BASE_FORMAT_F16:
    case SCE_GXM_COLOR_BASE_FORMAT_F32:
        return translate_swizzle1(static_cast<SceGxmColorSwizzle1Mode>(swizzle));

    case SCE_GXM_COLOR_BASE_FORMAT_U8U8:
    case SCE_GXM_COLOR_BASE_FORMAT_S8S8:
    case SCE_GXM_COLOR_BASE_FORMAT_U16U16:
    case SCE_GXM_COLOR_BASE_FORMAT_S16S16:
    case SCE_GXM_COLOR_BASE_FORMAT_F16F16:
    case SCE_GXM_COLOR_BASE_FORMAT_F32F32:
        return translate_swizzle2(static_cast<SceGxmColorSwizzle2Mode>(swizzle));

    case SCE_GXM_COLOR_BASE_FORMAT_U8U8U8:
    case SCE_GXM_COLOR_BASE_FORMAT_F11F11F10:
    case SCE_GXM_COLOR_BASE_FORMAT_SE5M9M9M9:
        return translate_swizzle3(static_cast<SceGxmColorSwizzle3Mode>(swizzle));

    case SCE_GXM_COLOR_BASE_FORMAT_U5U6U5:
        return translate_swizzle3_bgr(static_cast<SceGxmColorSwizzle3Mode>(swizzle));

    case SCE_GXM_COLOR_BASE_FORMAT_U8U8U8U8:
    case SCE_GXM_COLOR_BASE_FORMAT_S8S8S8S8:
    case SCE_GXM_COLOR_BASE_FORMAT_F16F16F16F16:
    // TODO: the swizzle for the following format is not fully supported
    case SCE_GXM_COLOR_BASE_FORMAT_U2U10U10U10:
    case SCE_GXM_COLOR_BASE_FORMAT_U2F10F10F10:
        return translate_swizzle4(static_cast<SceGxmColorSwizzle4Mode>(swizzle));

    case SCE_GXM_COLOR_BASE_FORMAT_U4U4U4U4:
    // TODO: the swizzle for the following format is not fully supported
    case SCE_GXM_COLOR_BASE_FORMAT_U1U5U5U5:
        return translate_swizzle4_abgr(static_cast<SceGxmColorSwizzle4Mode>(swizzle));

    default:
        LOG_ERROR("Unknown format {}", log_hex(base_format));
        return {};
    }
}

vk::Format translate_format(SceGxmColorBaseFormat format) {
    // TODO: look if all these formats are available on the GPU
    switch (format) {
    // classic unpacked formats
    case SCE_GXM_COLOR_BASE_FORMAT_U8:
        return vk::Format::eR8Unorm;
    case SCE_GXM_COLOR_BASE_FORMAT_S8:
        return vk::Format::eR8Snorm;
    case SCE_GXM_COLOR_BASE_FORMAT_U16:
        return vk::Format::eR16Unorm;
    case SCE_GXM_COLOR_BASE_FORMAT_S16:
        return vk::Format::eR16Snorm;
    case SCE_GXM_COLOR_BASE_FORMAT_F16:
        return vk::Format::eR16Sfloat;
    case SCE_GXM_COLOR_BASE_FORMAT_F32:
        return vk::Format::eR32Sfloat;

    case SCE_GXM_COLOR_BASE_FORMAT_U8U8:
        return vk::Format::eR8G8Unorm;
    case SCE_GXM_COLOR_BASE_FORMAT_S8S8:
        return vk::Format::eR8G8Snorm;
    case SCE_GXM_COLOR_BASE_FORMAT_U16U16:
        return vk::Format::eR16G16Unorm;
    case SCE_GXM_COLOR_BASE_FORMAT_S16S16:
        return vk::Format::eR16G16Snorm;
    case SCE_GXM_COLOR_BASE_FORMAT_F16F16:
        return vk::Format::eR16G16Sfloat;
    case SCE_GXM_COLOR_BASE_FORMAT_F32F32:
        return vk::Format::eR32G32Sfloat;

    case SCE_GXM_COLOR_BASE_FORMAT_U8U8U8U8:
        return vk::Format::eR8G8B8A8Unorm;
    case SCE_GXM_COLOR_BASE_FORMAT_S8S8S8S8:
        return vk::Format::eR8G8B8A8Snorm;
    case SCE_GXM_COLOR_BASE_FORMAT_F16F16F16F16:
        return vk::Format::eR16G16B16A16Sfloat;

    // packed formats
    case SCE_GXM_COLOR_BASE_FORMAT_U5U6U5:
        return vk::Format::eR5G6B5UnormPack16;
    case SCE_GXM_COLOR_BASE_FORMAT_F11F11F10:
        return vk::Format::eB10G11R11UfloatPack32;
    case SCE_GXM_COLOR_BASE_FORMAT_SE5M9M9M9:
        return vk::Format::eE5B9G9R9UfloatPack32;
    case SCE_GXM_COLOR_BASE_FORMAT_U8U8U8:
        // 24 bit packed RGB is not supported (on many GPUs), use rgba8 instead
        return vk::Format::eR8G8B8A8Unorm;

    case SCE_GXM_COLOR_BASE_FORMAT_U1U5U5U5:
        // TODO: we must use either eR5G5B5A1UnormPack16 or eA1R5G5B5UnormPack16 depending on the swizzle
        // also eR5G5B5A1UnormPack16 is not supported on all GPUs...
        return vk::Format::eA1R5G5B5UnormPack16;
    case SCE_GXM_COLOR_BASE_FORMAT_U4U4U4U4:
        return vk::Format::eR4G4B4A4UnormPack16;
    case SCE_GXM_COLOR_BASE_FORMAT_U2U10U10U10:
        // TODO: only ABGR or ARGB swizzle is supported with this format
        return vk::Format::eA2R10G10B10UnormPack32;
    case SCE_GXM_COLOR_BASE_FORMAT_U2F10F10F10:
        // This format is not supported on modern GPUs, give something bigger
        return vk::Format::eR16G16B16A16Sfloat;

    default:
        LOG_ERROR("Unknown format {}", log_hex(format));
        return {};
    }
}
} // namespace color

namespace texture {

static vk::ComponentMapping translate_swizzle1(SceGxmTextureSwizzle1Mode mode) {
    switch (mode) {
    case SCE_GXM_TEXTURE_SWIZZLE1_R:
        return swizzle_r001;
    case SCE_GXM_TEXTURE_SWIZZLE1_000R:
        return swizzle_r000;
    case SCE_GXM_TEXTURE_SWIZZLE1_111R:
        return swizzle_r111;
    case SCE_GXM_TEXTURE_SWIZZLE1_RRRR:
        return swizzle_rrrr;
    case SCE_GXM_TEXTURE_SWIZZLE1_0RRR:
        return swizzle_rrr0;
    case SCE_GXM_TEXTURE_SWIZZLE1_1RRR:
        return swizzle_rrr1;
    case SCE_GXM_TEXTURE_SWIZZLE1_R000:
        return swizzle_000r;
    case SCE_GXM_TEXTURE_SWIZZLE1_R111:
        return swizzle_111r;
    default:
        LOG_ERROR("Unknown swizzle mode {}", log_hex(mode));
        return {};
    }
}

static vk::ComponentMapping translate_swizzle2(SceGxmTextureSwizzle2Mode mode) {
    switch (mode) {
    case SCE_GXM_TEXTURE_SWIZZLE2_GR:
        return swizzle_rg01;
    case SCE_GXM_TEXTURE_SWIZZLE2_00GR:
        return swizzle_rg00;
    case SCE_GXM_TEXTURE_SWIZZLE2_GRRR:
        return swizzle_rrrg;
    case SCE_GXM_TEXTURE_SWIZZLE2_RGGG:
        return swizzle_gggr;
    case SCE_GXM_TEXTURE_SWIZZLE2_GRGR:
        return swizzle_rgrg;
    case SCE_GXM_TEXTURE_SWIZZLE2_00RG:
        return swizzle_gr00;
    default:
        LOG_ERROR("Unknown swizzle mode {}", log_hex(mode));
        return {};
    }
}

static vk::ComponentMapping translate_swizzleds(SceGxmTextureSwizzle2ModeAlt mode) {
    switch (mode) {
    case SCE_GXM_TEXTURE_SWIZZLE2_SD:
        return swizzle_identity;
    case SCE_GXM_TEXTURE_SWIZZLE2_DS:
        return swizzle_identity;
    default:
        LOG_ERROR("Unknown swizzle mode {}", log_hex(mode));
        return {};
    }
}

static vk::ComponentMapping translate_swizzle3(SceGxmTextureSwizzle3Mode mode) {
    switch (mode) {
    case SCE_GXM_TEXTURE_SWIZZLE3_BGR:
        return swizzle_rgb1;
    case SCE_GXM_TEXTURE_SWIZZLE3_RGB:
        return swizzle_bgr1;
    default:
        LOG_ERROR("Unknown swizzle mode {}", log_hex(mode));
        return {};
    }
}

// Convert the swizzle when the vulkan format has a BGR (or RGB packed) layout
static vk::ComponentMapping translate_swizzle3_bgr(SceGxmTextureSwizzle3Mode mode) {
    switch (mode) {
    case SCE_GXM_TEXTURE_SWIZZLE3_BGR:
        return swizzle_bgr1;
    case SCE_GXM_TEXTURE_SWIZZLE3_RGB:
        return swizzle_rgb1;
    default:
        LOG_ERROR("Unknown swizzle mode {}", log_hex(mode));
        return {};
    }
}

static vk::ComponentMapping translate_swizzle4(SceGxmTextureSwizzle4Mode mode) {
    switch (mode) {
    case SCE_GXM_TEXTURE_SWIZZLE4_ABGR:
        return swizzle_rgba;
    case SCE_GXM_TEXTURE_SWIZZLE4_ARGB:
        return swizzle_bgra;
    case SCE_GXM_TEXTURE_SWIZZLE4_RGBA:
        return swizzle_abgr;
    case SCE_GXM_TEXTURE_SWIZZLE4_BGRA:
        return swizzle_gbar;
    case SCE_GXM_TEXTURE_SWIZZLE4_1BGR:
        return swizzle_rgb1;
    case SCE_GXM_TEXTURE_SWIZZLE4_1RGB:
        return swizzle_bgr1;
    case SCE_GXM_TEXTURE_SWIZZLE4_RGB1:
        return swizzle_abg1;
    case SCE_GXM_TEXTURE_SWIZZLE4_BGR1:
        return swizzle_gba1;
    default:
        LOG_ERROR("Unknown swizzle mode {}", log_hex(mode));
        return {};
    }
}

// Convert the swizzle when the vulkan format has a ABGR (or RGBA packed) layout
static vk::ComponentMapping translate_swizzle4_abgr(SceGxmTextureSwizzle4Mode mode) {
    switch (mode) {
    case SCE_GXM_TEXTURE_SWIZZLE4_ABGR:
        return swizzle_abgr;
    case SCE_GXM_TEXTURE_SWIZZLE4_ARGB:
        return swizzle_gbar;
    case SCE_GXM_TEXTURE_SWIZZLE4_RGBA:
        return swizzle_rgba;
    case SCE_GXM_TEXTURE_SWIZZLE4_BGRA:
        return swizzle_bgra;
    default:
        LOG_ERROR("Unknown swizzle mode {}", log_hex(mode));
        return {};
    }
}

static vk::ComponentMapping translate_swizzleyuv420(SceGxmTextureSwizzleYUV420Mode mode) {
    switch (mode) {
    case SCE_GXM_TEXTURE_SWIZZLE_YUV_CSC0:
    case SCE_GXM_TEXTURE_SWIZZLE_YVU_CSC0:
    case SCE_GXM_TEXTURE_SWIZZLE_YUV_CSC1:
    case SCE_GXM_TEXTURE_SWIZZLE_YVU_CSC1:
        return swizzle_identity;
    default:
        LOG_ERROR("Unknown swizzle mode {}", log_hex(mode));
        return swizzle_identity;
    }
}

static vk::ComponentMapping translate_swizzleyuv422(SceGxmTextureSwizzleYUV422Mode mode) {
    switch (mode) {
    case SCE_GXM_TEXTURE_SWIZZLE_YUYV_CSC0:
    case SCE_GXM_TEXTURE_SWIZZLE_YVYU_CSC0:
    case SCE_GXM_TEXTURE_SWIZZLE_UYVY_CSC0:
    case SCE_GXM_TEXTURE_SWIZZLE_VYUY_CSC0:
    case SCE_GXM_TEXTURE_SWIZZLE_YUYV_CSC1:
    case SCE_GXM_TEXTURE_SWIZZLE_YVYU_CSC1:
    case SCE_GXM_TEXTURE_SWIZZLE_UYVY_CSC1:
    case SCE_GXM_TEXTURE_SWIZZLE_VYUY_CSC1:
        return swizzle_identity;
    default:
        LOG_ERROR("Unknown swizzle mode {}", log_hex(mode));
        return swizzle_identity;
    }
}

vk::ComponentMapping translate_swizzle(SceGxmTextureFormat format) {
    const SceGxmTextureBaseFormat base_format = gxm::get_base_format(format);
    const uint32_t swizzle = format & SCE_GXM_TEXTURE_SWIZZLE_MASK;
    switch (base_format) {
    // 1 Component.
    case SCE_GXM_TEXTURE_BASE_FORMAT_U8:
    case SCE_GXM_TEXTURE_BASE_FORMAT_S8:
    case SCE_GXM_TEXTURE_BASE_FORMAT_U16:
    case SCE_GXM_TEXTURE_BASE_FORMAT_S16:
    case SCE_GXM_TEXTURE_BASE_FORMAT_F16:
    case SCE_GXM_TEXTURE_BASE_FORMAT_U32:
    case SCE_GXM_TEXTURE_BASE_FORMAT_S32:
    case SCE_GXM_TEXTURE_BASE_FORMAT_F32:
    case SCE_GXM_TEXTURE_BASE_FORMAT_F32M:
    case SCE_GXM_TEXTURE_BASE_FORMAT_UBC4:
    case SCE_GXM_TEXTURE_BASE_FORMAT_SBC4:
        return translate_swizzle1(static_cast<SceGxmTextureSwizzle1Mode>(swizzle));

    // 2 components
    case SCE_GXM_TEXTURE_BASE_FORMAT_U8U8:
    case SCE_GXM_TEXTURE_BASE_FORMAT_S8S8:
    case SCE_GXM_TEXTURE_BASE_FORMAT_U16U16:
    case SCE_GXM_TEXTURE_BASE_FORMAT_S16S16:
    case SCE_GXM_TEXTURE_BASE_FORMAT_F16F16:
    case SCE_GXM_TEXTURE_BASE_FORMAT_U32U32:
    case SCE_GXM_TEXTURE_BASE_FORMAT_F32F32:
    case SCE_GXM_TEXTURE_BASE_FORMAT_UBC5:
    case SCE_GXM_TEXTURE_BASE_FORMAT_SBC5:
        return translate_swizzle2(static_cast<SceGxmTextureSwizzle2Mode>(swizzle));

    case SCE_GXM_TEXTURE_BASE_FORMAT_X8U24:
        return translate_swizzleds(static_cast<SceGxmTextureSwizzle2ModeAlt>(swizzle));

    // 3 components
    case SCE_GXM_TEXTURE_BASE_FORMAT_U8U8U8:
    case SCE_GXM_TEXTURE_BASE_FORMAT_S8S8S8:
    case SCE_GXM_TEXTURE_BASE_FORMAT_F11F11F10:
    case SCE_GXM_TEXTURE_BASE_FORMAT_SE5M9M9M9:
        return translate_swizzle3(static_cast<SceGxmTextureSwizzle3Mode>(swizzle));

    case SCE_GXM_TEXTURE_BASE_FORMAT_U5U6U5:
        return translate_swizzle3_bgr(static_cast<SceGxmTextureSwizzle3Mode>(swizzle));

    // 4 components
    case SCE_GXM_TEXTURE_BASE_FORMAT_U8U3U3U2:
    case SCE_GXM_TEXTURE_BASE_FORMAT_U8U8U8U8:
    case SCE_GXM_TEXTURE_BASE_FORMAT_S8S8S8S8:
    case SCE_GXM_TEXTURE_BASE_FORMAT_U16U16U16U16:
    case SCE_GXM_TEXTURE_BASE_FORMAT_S16S16S16S16:
    case SCE_GXM_TEXTURE_BASE_FORMAT_F16F16F16F16:
    case SCE_GXM_TEXTURE_BASE_FORMAT_P8:
    case SCE_GXM_TEXTURE_BASE_FORMAT_P4:
    case SCE_GXM_TEXTURE_BASE_FORMAT_PVRT2BPP:
    case SCE_GXM_TEXTURE_BASE_FORMAT_PVRT4BPP:
    case SCE_GXM_TEXTURE_BASE_FORMAT_PVRTII2BPP:
    case SCE_GXM_TEXTURE_BASE_FORMAT_PVRTII4BPP:
    // TODO: the following is not fully supported
    case SCE_GXM_TEXTURE_BASE_FORMAT_U2U10U10U10:
    case SCE_GXM_TEXTURE_BASE_FORMAT_U2F10F10F10:
    case SCE_GXM_TEXTURE_BASE_FORMAT_UBC1:
    case SCE_GXM_TEXTURE_BASE_FORMAT_UBC2:
    case SCE_GXM_TEXTURE_BASE_FORMAT_UBC3:
        return translate_swizzle4(static_cast<SceGxmTextureSwizzle4Mode>(swizzle));

    case SCE_GXM_TEXTURE_BASE_FORMAT_U4U4U4U4:
    // TODO: the following is not fully supported
    case SCE_GXM_TEXTURE_BASE_FORMAT_U1U5U5U5:
        return translate_swizzle4_abgr(static_cast<SceGxmTextureSwizzle4Mode>(swizzle));

    // YUV420.
    case SCE_GXM_TEXTURE_BASE_FORMAT_YUV420P2:
    case SCE_GXM_TEXTURE_BASE_FORMAT_YUV420P3:
        return translate_swizzleyuv420(static_cast<SceGxmTextureSwizzleYUV420Mode>(swizzle));

    // YUV422
    case SCE_GXM_TEXTURE_BASE_FORMAT_YUV422:
        return translate_swizzleyuv422(static_cast<SceGxmTextureSwizzleYUV422Mode>(swizzle));

    default:
        LOG_ERROR("Unknown format {}", log_hex(base_format));
        return {};
    }
}

vk::Format translate_format(SceGxmTextureBaseFormat base_format) {
    switch (base_format) {
    case SCE_GXM_TEXTURE_BASE_FORMAT_U8:
        return vk::Format::eR8Unorm;
    case SCE_GXM_TEXTURE_BASE_FORMAT_S8:
        return vk::Format::eR8Snorm;
    case SCE_GXM_TEXTURE_BASE_FORMAT_U16:
        return vk::Format::eR16Unorm;
    case SCE_GXM_TEXTURE_BASE_FORMAT_S16:
        return vk::Format::eR16Snorm;
    case SCE_GXM_TEXTURE_BASE_FORMAT_F16:
        return vk::Format::eR16Sfloat;
    case SCE_GXM_TEXTURE_BASE_FORMAT_U32:
        return vk::Format::eR32Uint;
    case SCE_GXM_TEXTURE_BASE_FORMAT_S32:
        return vk::Format::eR32Sint;
    case SCE_GXM_TEXTURE_BASE_FORMAT_X8U24:
    case SCE_GXM_TEXTURE_BASE_FORMAT_F32:
    case SCE_GXM_TEXTURE_BASE_FORMAT_F32M:
        return vk::Format::eR32Sfloat;

    case SCE_GXM_TEXTURE_BASE_FORMAT_U8U8:
        return vk::Format::eR8G8Unorm;
    case SCE_GXM_TEXTURE_BASE_FORMAT_S8S8:
        return vk::Format::eR8G8Snorm;
    case SCE_GXM_TEXTURE_BASE_FORMAT_U16U16:
        return vk::Format::eR16G16Unorm;
    case SCE_GXM_TEXTURE_BASE_FORMAT_S16S16:
        return vk::Format::eR16G16Snorm;
    case SCE_GXM_TEXTURE_BASE_FORMAT_F16F16:
        return vk::Format::eR16G16Sfloat;
    case SCE_GXM_TEXTURE_BASE_FORMAT_U32U32:
        return vk::Format::eR32G32Uint;
    case SCE_GXM_TEXTURE_BASE_FORMAT_F32F32:
        return vk::Format::eR32G32Sfloat;

    case SCE_GXM_TEXTURE_BASE_FORMAT_U8U8U8U8:
        return vk::Format::eR8G8B8A8Unorm;
    case SCE_GXM_TEXTURE_BASE_FORMAT_S8S8S8S8:
        return vk::Format::eR8G8B8A8Snorm;
    case SCE_GXM_TEXTURE_BASE_FORMAT_U16U16U16U16:
        return vk::Format::eR16G16B16A16Unorm;
    case SCE_GXM_TEXTURE_BASE_FORMAT_S16S16S16S16:
        return vk::Format::eR16G16B16A16Snorm;
    case SCE_GXM_TEXTURE_BASE_FORMAT_F16F16F16F16:
        return vk::Format::eR16G16B16A16Sfloat;

    case SCE_GXM_TEXTURE_BASE_FORMAT_U5U6U5:
        return vk::Format::eR5G6B5UnormPack16;
    case SCE_GXM_TEXTURE_BASE_FORMAT_F11F11F10:
        return vk::Format::eB10G11R11UfloatPack32;
    case SCE_GXM_TEXTURE_BASE_FORMAT_SE5M9M9M9:
        return vk::Format::eE5B9G9R9UfloatPack32;

    // the following formats are all decompressed to u8u8u8u8
    case SCE_GXM_TEXTURE_BASE_FORMAT_U8U3U3U2:
    case SCE_GXM_TEXTURE_BASE_FORMAT_U8U8U8:
    case SCE_GXM_TEXTURE_BASE_FORMAT_P8:
    case SCE_GXM_TEXTURE_BASE_FORMAT_P4:
    case SCE_GXM_TEXTURE_BASE_FORMAT_PVRT2BPP:
    case SCE_GXM_TEXTURE_BASE_FORMAT_PVRT4BPP:
    case SCE_GXM_TEXTURE_BASE_FORMAT_PVRTII2BPP:
    case SCE_GXM_TEXTURE_BASE_FORMAT_PVRTII4BPP:
    case SCE_GXM_TEXTURE_BASE_FORMAT_YUV420P2:
    case SCE_GXM_TEXTURE_BASE_FORMAT_YUV420P3:
    case SCE_GXM_TEXTURE_BASE_FORMAT_YUV422:
        return vk::Format::eR8G8B8A8Unorm;

    // Because of the lack of support of s8s8s8, an alpha channel is added to this texture
    case SCE_GXM_TEXTURE_BASE_FORMAT_S8S8S8:
        return vk::Format::eR8G8B8A8Snorm;

    case SCE_GXM_TEXTURE_BASE_FORMAT_U4U4U4U4:
        return vk::Format::eR4G4B4A4UnormPack16;
    case SCE_GXM_TEXTURE_BASE_FORMAT_U1U5U5U5:
        // TODO: same as for the color format
        return vk::Format::eA1R5G5B5UnormPack16;
    case SCE_GXM_TEXTURE_BASE_FORMAT_U2U10U10U10:
        // TODO: same as for the color format
        return vk::Format::eA2R10G10B10UnormPack32;
    case SCE_GXM_TEXTURE_BASE_FORMAT_U2F10F10F10:
        // not supported by modern GPUs
        return vk::Format::eR16G16B16A16Sfloat;

    case SCE_GXM_TEXTURE_BASE_FORMAT_UBC1:
        return vk::Format::eBc1RgbaUnormBlock;

    case SCE_GXM_TEXTURE_BASE_FORMAT_UBC2:
        return vk::Format::eBc2UnormBlock;

    case SCE_GXM_TEXTURE_BASE_FORMAT_UBC3:
        return vk::Format::eBc3UnormBlock;

    case SCE_GXM_TEXTURE_BASE_FORMAT_UBC4:
        return vk::Format::eBc4UnormBlock;

    case SCE_GXM_TEXTURE_BASE_FORMAT_SBC4:
        return vk::Format::eBc4SnormBlock;

    case SCE_GXM_TEXTURE_BASE_FORMAT_UBC5:
        return vk::Format::eBc5UnormBlock;

    case SCE_GXM_TEXTURE_BASE_FORMAT_SBC5:
        return vk::Format::eBc5SnormBlock;

    case SCE_GXM_TEXTURE_BASE_FORMAT_UBC6H:
        return vk::Format::eBc6HUfloatBlock;

    case SCE_GXM_TEXTURE_BASE_FORMAT_SBC6H:
        return vk::Format::eBc6HSfloatBlock;

    case SCE_GXM_TEXTURE_BASE_FORMAT_UBC7:
        return vk::Format::eBc7UnormBlock;

#define ASTC_FMT(b_x, b_y)                              \
    case SCE_GXM_TEXTURE_BASE_FORMAT_ASTC##b_x##x##b_y: \
        return vk::Format::eAstc##b_x##x##b_y##UnormBlock;

#include "../texture/astc_formats.inc"
#undef ASTC_FMT

    default:
        LOG_ERROR("Unknown format {}", log_hex(base_format));
        return {};
    }
}

vk::SamplerAddressMode translate_address_mode(SceGxmTextureAddrMode src) {
    switch (src) {
    case SCE_GXM_TEXTURE_ADDR_REPEAT:
        return vk::SamplerAddressMode::eRepeat;
    case SCE_GXM_TEXTURE_ADDR_MIRROR:
        return vk::SamplerAddressMode::eMirroredRepeat;
    case SCE_GXM_TEXTURE_ADDR_CLAMP:
        return vk::SamplerAddressMode::eClampToEdge;
    case SCE_GXM_TEXTURE_ADDR_MIRROR_CLAMP:
        return vk::SamplerAddressMode::eMirrorClampToEdge;
    case SCE_GXM_TEXTURE_ADDR_REPEAT_IGNORE_BORDER:
    case SCE_GXM_TEXTURE_ADDR_CLAMP_FULL_BORDER:
    case SCE_GXM_TEXTURE_ADDR_CLAMP_IGNORE_BORDER:
    case SCE_GXM_TEXTURE_ADDR_CLAMP_HALF_BORDER:
        LOG_ERROR_ONCE("Unhandled border color address mode, texture will be corrupted. Please report it to the developers.");
        return (src == SCE_GXM_TEXTURE_ADDR_REPEAT_IGNORE_BORDER) ? vk::SamplerAddressMode::eRepeat : vk::SamplerAddressMode::eClampToBorder;
    default:
        LOG_ERROR("Unknown address mode {}", log_hex(src));
        return vk::SamplerAddressMode::eClampToEdge;
    }
}

vk::Filter translate_filter(SceGxmTextureFilter src) {
    switch (src) {
    case SCE_GXM_TEXTURE_FILTER_POINT:
    case SCE_GXM_TEXTURE_FILTER_MIPMAP_POINT:
        return vk::Filter::eNearest;
    case SCE_GXM_TEXTURE_FILTER_LINEAR:
    case SCE_GXM_TEXTURE_FILTER_MIPMAP_LINEAR:
        return vk::Filter::eLinear;
    default:
        LOG_ERROR("Unknown texture filter {}", log_hex(src));
        return vk::Filter::eNearest;
    }
}
} // namespace texture
} // namespace renderer::vulkan
