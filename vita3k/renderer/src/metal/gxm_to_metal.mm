// Vita3K emulator project
// Copyright (C) 2026 Vita3K team
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

#include "gxm_to_metal.h"

#include <gxm/functions.h>
#include <util/log.h>

namespace renderer::metal {

MTLVertexFormat translate_attribute_format(SceGxmAttributeFormat format, unsigned int component_count, bool is_integer, bool is_signed) {
    if (component_count == 0 || component_count > 4 || format > SCE_GXM_ATTRIBUTE_FORMAT_UNTYPED) {
        LOG_ERROR("Unsupported attribute format {}x{}", log_hex(format), component_count);
        return MTLVertexFormatInvalid;
    }

    static constexpr MTLVertexFormat formats_integer[][4] = {
        /*SCE_GXM_ATTRIBUTE_FORMAT_U8*/ { MTLVertexFormatUChar, MTLVertexFormatUChar2, MTLVertexFormatUChar3, MTLVertexFormatUChar4 },
        /*SCE_GXM_ATTRIBUTE_FORMAT_S8*/ { MTLVertexFormatChar, MTLVertexFormatChar2, MTLVertexFormatChar3, MTLVertexFormatChar4 },
        /*SCE_GXM_ATTRIBUTE_FORMAT_U16*/ { MTLVertexFormatUShort, MTLVertexFormatUShort2, MTLVertexFormatUShort3, MTLVertexFormatUShort4 },
        /*SCE_GXM_ATTRIBUTE_FORMAT_S16*/ { MTLVertexFormatShort, MTLVertexFormatShort2, MTLVertexFormatShort3, MTLVertexFormatShort4 },
    };

    static constexpr MTLVertexFormat formats_float[][4] = {
        /*SCE_GXM_ATTRIBUTE_FORMAT_U8N*/ { MTLVertexFormatUCharNormalized, MTLVertexFormatUChar2Normalized, MTLVertexFormatUChar3Normalized, MTLVertexFormatUChar4Normalized },
        /*SCE_GXM_ATTRIBUTE_FORMAT_S8N*/ { MTLVertexFormatCharNormalized, MTLVertexFormatChar2Normalized, MTLVertexFormatChar3Normalized, MTLVertexFormatChar4Normalized },
        /*SCE_GXM_ATTRIBUTE_FORMAT_U16N*/ { MTLVertexFormatUShortNormalized, MTLVertexFormatUShort2Normalized, MTLVertexFormatUShort3Normalized, MTLVertexFormatUShort4Normalized },
        /*SCE_GXM_ATTRIBUTE_FORMAT_S16N*/ { MTLVertexFormatShortNormalized, MTLVertexFormatShort2Normalized, MTLVertexFormatShort3Normalized, MTLVertexFormatShort4Normalized },
        /*SCE_GXM_ATTRIBUTE_FORMAT_F16*/ { MTLVertexFormatHalf, MTLVertexFormatHalf2, MTLVertexFormatHalf3, MTLVertexFormatHalf4 },
        /*SCE_GXM_ATTRIBUTE_FORMAT_F32*/ { MTLVertexFormatFloat, MTLVertexFormatFloat2, MTLVertexFormatFloat3, MTLVertexFormatFloat4 },
    };

    static constexpr MTLVertexFormat formats_untyped[][4] = {
        /*SCE_GXM_ATTRIBUTE_FORMAT_UNTYPED unsigned*/ { MTLVertexFormatUInt, MTLVertexFormatUInt2, MTLVertexFormatUInt3, MTLVertexFormatUInt4 },
        /*SCE_GXM_ATTRIBUTE_FORMAT_UNTYPED signed*/ { MTLVertexFormatInt, MTLVertexFormatInt2, MTLVertexFormatInt3, MTLVertexFormatInt4 },
    };

    const int format_idx = format;
    if (format_idx < SCE_GXM_ATTRIBUTE_FORMAT_U8N) {
        // Metal has no scaled formats, integers are converted to floats in the shader
        return formats_integer[format_idx][component_count - 1];
    } else if (format == SCE_GXM_ATTRIBUTE_FORMAT_UNTYPED) {
        return formats_untyped[is_signed][component_count - 1];
    } else {
        return formats_float[format_idx - SCE_GXM_ATTRIBUTE_FORMAT_U8N][component_count - 1];
    }
}

MTLBlendFactor translate_blend_factor(const SceGxmBlendFactor blend_factor) {
    switch (blend_factor) {
    case SCE_GXM_BLEND_FACTOR_ZERO:
        return MTLBlendFactorZero;
    case SCE_GXM_BLEND_FACTOR_ONE:
        return MTLBlendFactorOne;
    case SCE_GXM_BLEND_FACTOR_SRC_COLOR:
        return MTLBlendFactorSourceColor;
    case SCE_GXM_BLEND_FACTOR_ONE_MINUS_SRC_COLOR:
        return MTLBlendFactorOneMinusSourceColor;
    case SCE_GXM_BLEND_FACTOR_SRC_ALPHA:
        return MTLBlendFactorSourceAlpha;
    case SCE_GXM_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA:
        return MTLBlendFactorOneMinusSourceAlpha;
    case SCE_GXM_BLEND_FACTOR_DST_COLOR:
        return MTLBlendFactorDestinationColor;
    case SCE_GXM_BLEND_FACTOR_ONE_MINUS_DST_COLOR:
        return MTLBlendFactorOneMinusDestinationColor;
    case SCE_GXM_BLEND_FACTOR_DST_ALPHA:
        return MTLBlendFactorDestinationAlpha;
    case SCE_GXM_BLEND_FACTOR_ONE_MINUS_DST_ALPHA:
        return MTLBlendFactorOneMinusDestinationAlpha;
    case SCE_GXM_BLEND_FACTOR_SRC_ALPHA_SATURATE:
        return MTLBlendFactorSourceAlphaSaturated;
    case SCE_GXM_BLEND_FACTOR_DST_ALPHA_SATURATE:
        return MTLBlendFactorDestinationAlpha; // TODO Not supported.
    default:
        LOG_ERROR("Unknown blend factor: {}", fmt::underlying(blend_factor));
        return MTLBlendFactorOne;
    }
}

MTLBlendOperation translate_blend_func(const SceGxmBlendFunc blend_func) {
    switch (blend_func) {
    case SCE_GXM_BLEND_FUNC_NONE:
    case SCE_GXM_BLEND_FUNC_ADD:
        return MTLBlendOperationAdd;
    case SCE_GXM_BLEND_FUNC_SUBTRACT:
        return MTLBlendOperationSubtract;
    case SCE_GXM_BLEND_FUNC_REVERSE_SUBTRACT:
        return MTLBlendOperationReverseSubtract;
    case SCE_GXM_BLEND_FUNC_MIN:
        return MTLBlendOperationMin;
    case SCE_GXM_BLEND_FUNC_MAX:
        return MTLBlendOperationMax;
    default:
        LOG_ERROR("Unknown blend func: {}", fmt::underlying(blend_func));
        return MTLBlendOperationAdd;
    }
}

MTLPrimitiveType translate_primitive(SceGxmPrimitiveType primitive) {
    switch (primitive) {
    case SCE_GXM_PRIMITIVE_TRIANGLES:
        return MTLPrimitiveTypeTriangle;
    case SCE_GXM_PRIMITIVE_TRIANGLE_STRIP:
        return MTLPrimitiveTypeTriangleStrip;
    case SCE_GXM_PRIMITIVE_TRIANGLE_FAN:
        // fans are expanded into triangle lists before drawing
        return MTLPrimitiveTypeTriangle;
    case SCE_GXM_PRIMITIVE_LINES:
        return MTLPrimitiveTypeLine;
    case SCE_GXM_PRIMITIVE_POINTS:
        return MTLPrimitiveTypePoint;
    case SCE_GXM_PRIMITIVE_TRIANGLE_EDGES:
        LOG_WARN("Unsupported primitive: SCE_GXM_PRIMITIVE_TRIANGLE_EDGES, using SCE_GXM_PRIMITIVE_TRIANGLES instead");
        return MTLPrimitiveTypeTriangle;
    default:
        LOG_ERROR("Unknown primitive: {}", fmt::underlying(primitive));
        return MTLPrimitiveTypeTriangle;
    }
}

MTLCompareFunction translate_depth_func(SceGxmDepthFunc depth_func) {
    switch (depth_func) {
    case SCE_GXM_DEPTH_FUNC_NEVER:
        return MTLCompareFunctionNever;
    case SCE_GXM_DEPTH_FUNC_LESS:
        return MTLCompareFunctionLess;
    case SCE_GXM_DEPTH_FUNC_EQUAL:
        return MTLCompareFunctionEqual;
    case SCE_GXM_DEPTH_FUNC_LESS_EQUAL:
        return MTLCompareFunctionLessEqual;
    case SCE_GXM_DEPTH_FUNC_GREATER:
        return MTLCompareFunctionGreater;
    case SCE_GXM_DEPTH_FUNC_NOT_EQUAL:
        return MTLCompareFunctionNotEqual;
    case SCE_GXM_DEPTH_FUNC_GREATER_EQUAL:
        return MTLCompareFunctionGreaterEqual;
    case SCE_GXM_DEPTH_FUNC_ALWAYS:
        return MTLCompareFunctionAlways;
    default:
        LOG_ERROR("Unknown depth func {}", log_hex(depth_func));
        return MTLCompareFunctionAlways;
    }
}

MTLTriangleFillMode translate_polygon_mode(SceGxmPolygonMode polygon_mode) {
    switch (polygon_mode) {
    case SCE_GXM_POLYGON_MODE_TRIANGLE_FILL:
        return MTLTriangleFillModeFill;
    case SCE_GXM_POLYGON_MODE_LINE:
    case SCE_GXM_POLYGON_MODE_TRIANGLE_LINE:
        return MTLTriangleFillModeLines;
    case SCE_GXM_POLYGON_MODE_POINT_10UV:
    case SCE_GXM_POLYGON_MODE_POINT:
    case SCE_GXM_POLYGON_MODE_POINT_01UV:
    case SCE_GXM_POLYGON_MODE_TRIANGLE_POINT:
        LOG_WARN_ONCE("Point polygon mode is not supported by Metal, using fill mode");
        return MTLTriangleFillModeFill;
    default:
        LOG_ERROR("Unknown polygon mode {}", log_hex(polygon_mode));
        return MTLTriangleFillModeFill;
    }
}

MTLCullMode translate_cull_mode(SceGxmCullMode cull_mode) {
    // In gxm, the front face is always counter clockwise
    switch (cull_mode) {
    case SCE_GXM_CULL_NONE:
        return MTLCullModeNone;
    case SCE_GXM_CULL_CW:
        return MTLCullModeBack;
    case SCE_GXM_CULL_CCW:
        return MTLCullModeFront;
    default:
        LOG_ERROR("Unknown cull mode {}", log_hex(cull_mode));
        return MTLCullModeNone;
    }
}

MTLCompareFunction translate_stencil_func(SceGxmStencilFunc stencil_func) {
    switch (stencil_func) {
    case SCE_GXM_STENCIL_FUNC_NEVER:
        return MTLCompareFunctionNever;
    case SCE_GXM_STENCIL_FUNC_LESS:
        return MTLCompareFunctionLess;
    case SCE_GXM_STENCIL_FUNC_EQUAL:
        return MTLCompareFunctionEqual;
    case SCE_GXM_STENCIL_FUNC_LESS_EQUAL:
        return MTLCompareFunctionLessEqual;
    case SCE_GXM_STENCIL_FUNC_GREATER:
        return MTLCompareFunctionGreater;
    case SCE_GXM_STENCIL_FUNC_NOT_EQUAL:
        return MTLCompareFunctionNotEqual;
    case SCE_GXM_STENCIL_FUNC_GREATER_EQUAL:
        return MTLCompareFunctionGreaterEqual;
    case SCE_GXM_STENCIL_FUNC_ALWAYS:
        return MTLCompareFunctionAlways;
    default:
        LOG_ERROR("Unknown stencil func {}", log_hex(stencil_func));
        return MTLCompareFunctionAlways;
    }
}

MTLStencilOperation translate_stencil_op(SceGxmStencilOp stencil_op) {
    switch (stencil_op) {
    case SCE_GXM_STENCIL_OP_KEEP:
        return MTLStencilOperationKeep;
    case SCE_GXM_STENCIL_OP_ZERO:
        return MTLStencilOperationZero;
    case SCE_GXM_STENCIL_OP_REPLACE:
        return MTLStencilOperationReplace;
    case SCE_GXM_STENCIL_OP_INCR:
        return MTLStencilOperationIncrementClamp;
    case SCE_GXM_STENCIL_OP_DECR:
        return MTLStencilOperationDecrementClamp;
    case SCE_GXM_STENCIL_OP_INVERT:
        return MTLStencilOperationInvert;
    case SCE_GXM_STENCIL_OP_INCR_WRAP:
        return MTLStencilOperationIncrementWrap;
    case SCE_GXM_STENCIL_OP_DECR_WRAP:
        return MTLStencilOperationDecrementWrap;
    default:
        LOG_ERROR("Unknown stencil op {}", log_hex(stencil_op));
        return MTLStencilOperationKeep;
    }
}

bool swizzle_equal(const MTLTextureSwizzleChannels &a, const MTLTextureSwizzleChannels &b) {
    return a.red == b.red && a.green == b.green && a.blue == b.blue && a.alpha == b.alpha;
}

bool swizzle_is_identity(const MTLTextureSwizzleChannels &swizzle) {
    return swizzle_equal(swizzle, identity_swizzle);
}

MTLTextureSwizzleChannels color_to_texture_swizzle(const MTLTextureSwizzleChannels &swizzle_color, const MTLTextureSwizzleChannels &swizzle_texture) {
    MTLTextureSwizzleChannels result_comp;
    MTLTextureSwizzle *result = reinterpret_cast<MTLTextureSwizzle *>(&result_comp);
    const MTLTextureSwizzle *color = reinterpret_cast<const MTLTextureSwizzle *>(&swizzle_color);
    const MTLTextureSwizzle *texture = reinterpret_cast<const MTLTextureSwizzle *>(&swizzle_texture);

    for (int i = 0; i < 4; i++) {
        const MTLTextureSwizzle component = texture[i];
        if (component < MTLTextureSwizzleRed) {
            result[i] = component;
        } else {
            result[i] = MTLTextureSwizzleZero;
            // look in the color swizzle if there is a component writing to the same channel
            for (int j = 0; j < 4; j++) {
                if (component == color[j])
                    result[i] = static_cast<MTLTextureSwizzle>(MTLTextureSwizzleRed + j);
            }
        }
    }
    return result_comp;
}

uint32_t bytes_per_pixel(MTLPixelFormat format) {
    switch (format) {
    case MTLPixelFormatR8Unorm:
    case MTLPixelFormatR8Snorm:
    case MTLPixelFormatR8Unorm_sRGB:
    case MTLPixelFormatR8Uint:
    case MTLPixelFormatR8Sint:
    case MTLPixelFormatStencil8:
        return 1;
    case MTLPixelFormatR16Unorm:
    case MTLPixelFormatR16Snorm:
    case MTLPixelFormatR16Float:
    case MTLPixelFormatR16Uint:
    case MTLPixelFormatR16Sint:
    case MTLPixelFormatRG8Unorm:
    case MTLPixelFormatRG8Snorm:
    case MTLPixelFormatRG8Unorm_sRGB:
    case MTLPixelFormatB5G6R5Unorm:
    case MTLPixelFormatA1BGR5Unorm:
    case MTLPixelFormatABGR4Unorm:
    case MTLPixelFormatBGR5A1Unorm:
    case MTLPixelFormatDepth16Unorm:
        return 2;
    case MTLPixelFormatRGBA16Unorm:
    case MTLPixelFormatRGBA16Snorm:
    case MTLPixelFormatRGBA16Float:
    case MTLPixelFormatRGBA16Uint:
    case MTLPixelFormatRGBA16Sint:
    case MTLPixelFormatRG32Float:
    case MTLPixelFormatRG32Uint:
    case MTLPixelFormatRG32Sint:
    case MTLPixelFormatDepth32Float_Stencil8:
    case MTLPixelFormatX32_Stencil8:
        return 8;
    case MTLPixelFormatRGBA32Float:
    case MTLPixelFormatRGBA32Uint:
    case MTLPixelFormatRGBA32Sint:
        return 16;
    default:
        return 4;
    }
}

uint32_t component_count(MTLPixelFormat format) {
    switch (format) {
    case MTLPixelFormatR8Unorm:
    case MTLPixelFormatR8Snorm:
    case MTLPixelFormatR8Unorm_sRGB:
    case MTLPixelFormatR8Uint:
    case MTLPixelFormatR8Sint:
    case MTLPixelFormatR16Unorm:
    case MTLPixelFormatR16Snorm:
    case MTLPixelFormatR16Float:
    case MTLPixelFormatR16Uint:
    case MTLPixelFormatR16Sint:
    case MTLPixelFormatR32Float:
    case MTLPixelFormatR32Uint:
    case MTLPixelFormatR32Sint:
    case MTLPixelFormatDepth32Float:
    case MTLPixelFormatStencil8:
        return 1;
    case MTLPixelFormatRG8Unorm:
    case MTLPixelFormatRG8Snorm:
    case MTLPixelFormatRG8Unorm_sRGB:
    case MTLPixelFormatRG16Unorm:
    case MTLPixelFormatRG16Snorm:
    case MTLPixelFormatRG16Float:
    case MTLPixelFormatRG32Float:
    case MTLPixelFormatRG32Uint:
    case MTLPixelFormatRG32Sint:
    case MTLPixelFormatDepth32Float_Stencil8:
    case MTLPixelFormatX32_Stencil8:
        return 2;
    case MTLPixelFormatB5G6R5Unorm:
    case MTLPixelFormatRG11B10Float:
    case MTLPixelFormatRGB9E5Float:
        return 3;
    default:
        return 4;
    }
}

MTLPixelFormat linear_to_srgb(MTLPixelFormat format) {
    switch (format) {
    case MTLPixelFormatR8Unorm:
        return MTLPixelFormatR8Unorm_sRGB;
    case MTLPixelFormatRG8Unorm:
        return MTLPixelFormatRG8Unorm_sRGB;
    case MTLPixelFormatRGBA8Unorm:
        return MTLPixelFormatRGBA8Unorm_sRGB;
    case MTLPixelFormatBGRA8Unorm:
        return MTLPixelFormatBGRA8Unorm_sRGB;
    case MTLPixelFormatBC1_RGBA:
        return MTLPixelFormatBC1_RGBA_sRGB;
    case MTLPixelFormatBC2_RGBA:
        return MTLPixelFormatBC2_RGBA_sRGB;
    case MTLPixelFormatBC3_RGBA:
        return MTLPixelFormatBC3_RGBA_sRGB;
    case MTLPixelFormatBC7_RGBAUnorm:
        return MTLPixelFormatBC7_RGBAUnorm_sRGB;
#define ASTC_FMT(b_x, b_y)                           \
    case MTLPixelFormatASTC_##b_x##x##b_y##_LDR: \
        return MTLPixelFormatASTC_##b_x##x##b_y##_sRGB;
#include "../texture/astc_formats.inc"
#undef ASTC_FMT
    default:
        LOG_WARN_ONCE("Trying to use gamma correction with non-compatible format {}", format_name(format));
        return format;
    }
}

bool is_srgb_format(MTLPixelFormat format) {
    switch (format) {
    case MTLPixelFormatR8Unorm_sRGB:
    case MTLPixelFormatRG8Unorm_sRGB:
    case MTLPixelFormatRGBA8Unorm_sRGB:
    case MTLPixelFormatBGRA8Unorm_sRGB:
    case MTLPixelFormatBC1_RGBA_sRGB:
    case MTLPixelFormatBC2_RGBA_sRGB:
    case MTLPixelFormatBC3_RGBA_sRGB:
    case MTLPixelFormatBC7_RGBAUnorm_sRGB:
        return true;
    default:
        return false;
    }
}

bool is_depth_stencil_format(MTLPixelFormat format) {
    switch (format) {
    case MTLPixelFormatDepth16Unorm:
    case MTLPixelFormatDepth32Float:
    case MTLPixelFormatStencil8:
    case MTLPixelFormatDepth32Float_Stencil8:
    case MTLPixelFormatX32_Stencil8:
        return true;
    default:
        return false;
    }
}

const char *format_name(MTLPixelFormat format) {
    switch (format) {
    case MTLPixelFormatR8Unorm: return "R8Unorm";
    case MTLPixelFormatR8Snorm: return "R8Snorm";
    case MTLPixelFormatR8Unorm_sRGB: return "R8Unorm_sRGB";
    case MTLPixelFormatR16Unorm: return "R16Unorm";
    case MTLPixelFormatR16Snorm: return "R16Snorm";
    case MTLPixelFormatR16Float: return "R16Float";
    case MTLPixelFormatR32Uint: return "R32Uint";
    case MTLPixelFormatR32Sint: return "R32Sint";
    case MTLPixelFormatR32Float: return "R32Float";
    case MTLPixelFormatRG8Unorm: return "RG8Unorm";
    case MTLPixelFormatRG8Snorm: return "RG8Snorm";
    case MTLPixelFormatRG8Unorm_sRGB: return "RG8Unorm_sRGB";
    case MTLPixelFormatRG16Unorm: return "RG16Unorm";
    case MTLPixelFormatRG16Snorm: return "RG16Snorm";
    case MTLPixelFormatRG16Float: return "RG16Float";
    case MTLPixelFormatRG32Uint: return "RG32Uint";
    case MTLPixelFormatRG32Float: return "RG32Float";
    case MTLPixelFormatRGBA8Unorm: return "RGBA8Unorm";
    case MTLPixelFormatRGBA8Unorm_sRGB: return "RGBA8Unorm_sRGB";
    case MTLPixelFormatRGBA8Snorm: return "RGBA8Snorm";
    case MTLPixelFormatBGRA8Unorm: return "BGRA8Unorm";
    case MTLPixelFormatBGRA8Unorm_sRGB: return "BGRA8Unorm_sRGB";
    case MTLPixelFormatRGBA16Unorm: return "RGBA16Unorm";
    case MTLPixelFormatRGBA16Snorm: return "RGBA16Snorm";
    case MTLPixelFormatRGBA16Float: return "RGBA16Float";
    case MTLPixelFormatB5G6R5Unorm: return "B5G6R5Unorm";
    case MTLPixelFormatA1BGR5Unorm: return "A1BGR5Unorm";
    case MTLPixelFormatABGR4Unorm: return "ABGR4Unorm";
    case MTLPixelFormatBGR5A1Unorm: return "BGR5A1Unorm";
    case MTLPixelFormatRG11B10Float: return "RG11B10Float";
    case MTLPixelFormatRGB9E5Float: return "RGB9E5Float";
    case MTLPixelFormatBGR10A2Unorm: return "BGR10A2Unorm";
    case MTLPixelFormatDepth32Float_Stencil8: return "Depth32Float_Stencil8";
    case MTLPixelFormatX32_Stencil8: return "X32_Stencil8";
    default: return "other";
    }
}

using Swz = MTLTextureSwizzle;
#define SWZ(r, g, b, a) \
    MTLTextureSwizzleChannels { MTLTextureSwizzle##r, MTLTextureSwizzle##g, MTLTextureSwizzle##b, MTLTextureSwizzle##a }

static constexpr MTLTextureSwizzleChannels swizzle_identity = SWZ(Red, Green, Blue, Alpha);

static constexpr MTLTextureSwizzleChannels swizzle_r001 = SWZ(Red, Zero, Zero, One);
static constexpr MTLTextureSwizzleChannels swizzle_r000 = SWZ(Red, Zero, Zero, Zero);
static constexpr MTLTextureSwizzleChannels swizzle_r111 = SWZ(Red, One, One, One);
static constexpr MTLTextureSwizzleChannels swizzle_rrrr = SWZ(Red, Red, Red, Red);
static constexpr MTLTextureSwizzleChannels swizzle_rrr0 = SWZ(Red, Red, Red, Zero);
static constexpr MTLTextureSwizzleChannels swizzle_rrr1 = SWZ(Red, Red, Red, One);
static constexpr MTLTextureSwizzleChannels swizzle_000r = SWZ(Zero, Zero, Zero, Red);
static constexpr MTLTextureSwizzleChannels swizzle_111r = SWZ(One, One, One, Red);

static constexpr MTLTextureSwizzleChannels swizzle_rg01 = SWZ(Red, Green, Zero, One);
static constexpr MTLTextureSwizzleChannels swizzle_gr01 = SWZ(Green, Red, Zero, One);
static constexpr MTLTextureSwizzleChannels swizzle_rg00 = SWZ(Red, Green, Zero, Zero);
static constexpr MTLTextureSwizzleChannels swizzle_rrrg = SWZ(Red, Red, Red, Green);
static constexpr MTLTextureSwizzleChannels swizzle_gggr = SWZ(Green, Green, Green, Red);
static constexpr MTLTextureSwizzleChannels swizzle_rgrg = SWZ(Red, Green, Red, Green);
static constexpr MTLTextureSwizzleChannels swizzle_gr00 = SWZ(Green, Red, Zero, Zero);

static constexpr MTLTextureSwizzleChannels swizzle_bgr1 = SWZ(Blue, Green, Red, One);
static constexpr MTLTextureSwizzleChannels swizzle_rgb1 = SWZ(Red, Green, Blue, One);

static constexpr MTLTextureSwizzleChannels swizzle_rgba = SWZ(Red, Green, Blue, Alpha);
static constexpr MTLTextureSwizzleChannels swizzle_bgra = SWZ(Blue, Green, Red, Alpha);
static constexpr MTLTextureSwizzleChannels swizzle_abgr = SWZ(Alpha, Blue, Green, Red);
static constexpr MTLTextureSwizzleChannels swizzle_gbar = SWZ(Green, Blue, Alpha, Red);
static constexpr MTLTextureSwizzleChannels swizzle_abg1 = SWZ(Alpha, Blue, Green, One);
static constexpr MTLTextureSwizzleChannels swizzle_gba1 = SWZ(Green, Blue, Alpha, One);

namespace color {

static MTLTextureSwizzleChannels translate_swizzle1(SceGxmColorSwizzle1Mode mode) {
    switch (mode) {
    case SCE_GXM_COLOR_SWIZZLE1_R:
        return swizzle_r001;
    default:
        LOG_ERROR("Unknown swizzle mode {}", log_hex(mode));
        return swizzle_identity;
    }
}

static MTLTextureSwizzleChannels translate_swizzle2(SceGxmColorSwizzle2Mode mode) {
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

static MTLTextureSwizzleChannels translate_swizzle3(SceGxmColorSwizzle3Mode mode) {
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

static MTLTextureSwizzleChannels translate_swizzle3_bgr(SceGxmColorSwizzle3Mode mode) {
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

static MTLTextureSwizzleChannels translate_swizzle4(SceGxmColorSwizzle4Mode mode) {
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

static MTLTextureSwizzleChannels translate_swizzle4_abgr(SceGxmColorSwizzle4Mode mode) {
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
        return swizzle_identity;
    }
}

static MTLTextureSwizzleChannels translate_swizzle4_a1r5g5b5(SceGxmColorSwizzle4Mode mode) {
    switch (mode) {
    case SCE_GXM_COLOR_SWIZZLE4_ABGR:
    case SCE_GXM_COLOR_SWIZZLE4_BGRA:
        return swizzle_bgra;
    case SCE_GXM_COLOR_SWIZZLE4_ARGB:
    case SCE_GXM_COLOR_SWIZZLE4_RGBA:
        return swizzle_rgba;
    default:
        LOG_ERROR("Unknown swizzle mode {}", log_hex(mode));
        return swizzle_identity;
    }
}

MTLTextureSwizzleChannels translate_swizzle(SceGxmColorFormat format) {
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
    case SCE_GXM_COLOR_BASE_FORMAT_U2U10U10U10:
    case SCE_GXM_COLOR_BASE_FORMAT_U2F10F10F10:
        return translate_swizzle4(static_cast<SceGxmColorSwizzle4Mode>(swizzle));

    case SCE_GXM_COLOR_BASE_FORMAT_U4U4U4U4:
        return translate_swizzle4_abgr(static_cast<SceGxmColorSwizzle4Mode>(swizzle));

    case SCE_GXM_COLOR_BASE_FORMAT_U1U5U5U5:
        return translate_swizzle4_a1r5g5b5(static_cast<SceGxmColorSwizzle4Mode>(swizzle));

    default:
        LOG_ERROR("Unknown format {}", log_hex(base_format));
        return swizzle_identity;
    }
}

MTLPixelFormat translate_format(SceGxmColorBaseFormat format, bool support_packed16) {
    switch (format) {
    case SCE_GXM_COLOR_BASE_FORMAT_U8:
        return MTLPixelFormatR8Unorm;
    case SCE_GXM_COLOR_BASE_FORMAT_S8:
        return MTLPixelFormatR8Snorm;
    case SCE_GXM_COLOR_BASE_FORMAT_U16:
        return MTLPixelFormatR16Unorm;
    case SCE_GXM_COLOR_BASE_FORMAT_S16:
        return MTLPixelFormatR16Snorm;
    case SCE_GXM_COLOR_BASE_FORMAT_F16:
        return MTLPixelFormatR16Float;
    case SCE_GXM_COLOR_BASE_FORMAT_F32:
        return MTLPixelFormatR32Float;

    case SCE_GXM_COLOR_BASE_FORMAT_U8U8:
        return MTLPixelFormatRG8Unorm;
    case SCE_GXM_COLOR_BASE_FORMAT_S8S8:
        return MTLPixelFormatRG8Snorm;
    case SCE_GXM_COLOR_BASE_FORMAT_U16U16:
        return MTLPixelFormatRG16Unorm;
    case SCE_GXM_COLOR_BASE_FORMAT_S16S16:
        return MTLPixelFormatRG16Snorm;
    case SCE_GXM_COLOR_BASE_FORMAT_F16F16:
        return MTLPixelFormatRG16Float;
    case SCE_GXM_COLOR_BASE_FORMAT_F32F32:
        return MTLPixelFormatRG32Float;

    case SCE_GXM_COLOR_BASE_FORMAT_U8U8U8U8:
        return MTLPixelFormatRGBA8Unorm;
    case SCE_GXM_COLOR_BASE_FORMAT_S8S8S8S8:
        return MTLPixelFormatRGBA8Snorm;
    case SCE_GXM_COLOR_BASE_FORMAT_F16F16F16F16:
        return MTLPixelFormatRGBA16Float;

    case SCE_GXM_COLOR_BASE_FORMAT_U5U6U5:
        return support_packed16 ? MTLPixelFormatB5G6R5Unorm : MTLPixelFormatRGBA8Unorm;
    case SCE_GXM_COLOR_BASE_FORMAT_F11F11F10:
        return MTLPixelFormatRG11B10Float;
    case SCE_GXM_COLOR_BASE_FORMAT_SE5M9M9M9:
        return MTLPixelFormatRGB9E5Float;
    case SCE_GXM_COLOR_BASE_FORMAT_U8U8U8:
        // 24 bit packed RGB is not supported, use rgba8 instead
        return MTLPixelFormatRGBA8Unorm;

    case SCE_GXM_COLOR_BASE_FORMAT_U1U5U5U5:
        return support_packed16 ? MTLPixelFormatBGR5A1Unorm : MTLPixelFormatRGBA8Unorm;
    case SCE_GXM_COLOR_BASE_FORMAT_U4U4U4U4:
        return support_packed16 ? MTLPixelFormatABGR4Unorm : MTLPixelFormatRGBA8Unorm;
    case SCE_GXM_COLOR_BASE_FORMAT_U2U10U10U10:
        return MTLPixelFormatBGR10A2Unorm;
    case SCE_GXM_COLOR_BASE_FORMAT_U2F10F10F10:
        // This format is not supported on modern GPUs, give something bigger
        return MTLPixelFormatRGBA16Float;

    default:
        LOG_ERROR("Unknown format {}", log_hex(format));
        return MTLPixelFormatRGBA8Unorm;
    }
}
} // namespace color

namespace texture {

static MTLTextureSwizzleChannels translate_swizzle1(SceGxmTextureSwizzle1Mode mode) {
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
        return swizzle_identity;
    }
}

static MTLTextureSwizzleChannels translate_swizzle2(SceGxmTextureSwizzle2Mode mode) {
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
        return swizzle_identity;
    }
}

static MTLTextureSwizzleChannels translate_swizzle3(SceGxmTextureSwizzle3Mode mode) {
    switch (mode) {
    case SCE_GXM_TEXTURE_SWIZZLE3_BGR:
        return swizzle_rgb1;
    case SCE_GXM_TEXTURE_SWIZZLE3_RGB:
        return swizzle_bgr1;
    default:
        LOG_ERROR("Unknown swizzle mode {}", log_hex(mode));
        return swizzle_identity;
    }
}

static MTLTextureSwizzleChannels translate_swizzle3_bgr(SceGxmTextureSwizzle3Mode mode) {
    switch (mode) {
    case SCE_GXM_TEXTURE_SWIZZLE3_BGR:
        return swizzle_bgr1;
    case SCE_GXM_TEXTURE_SWIZZLE3_RGB:
        return swizzle_rgb1;
    default:
        LOG_ERROR("Unknown swizzle mode {}", log_hex(mode));
        return swizzle_identity;
    }
}

static MTLTextureSwizzleChannels translate_swizzle4(SceGxmTextureSwizzle4Mode mode) {
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
        return swizzle_identity;
    }
}

static MTLTextureSwizzleChannels translate_swizzle4_abgr(SceGxmTextureSwizzle4Mode mode) {
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
        return swizzle_identity;
    }
}

static MTLTextureSwizzleChannels translate_swizzle4_a1r5g5b5(SceGxmTextureSwizzle4Mode mode) {
    switch (mode) {
    case SCE_GXM_TEXTURE_SWIZZLE4_ABGR:
    case SCE_GXM_TEXTURE_SWIZZLE4_BGRA:
        return swizzle_bgra;
    case SCE_GXM_TEXTURE_SWIZZLE4_ARGB:
    case SCE_GXM_TEXTURE_SWIZZLE4_RGBA:
        return swizzle_rgba;
    case SCE_GXM_TEXTURE_SWIZZLE4_1BGR:
    case SCE_GXM_TEXTURE_SWIZZLE4_BGR1:
        return swizzle_bgr1;
    case SCE_GXM_TEXTURE_SWIZZLE4_1RGB:
    case SCE_GXM_TEXTURE_SWIZZLE4_RGB1:
        return swizzle_rgb1;
    default:
        LOG_ERROR("Unknown swizzle mode {}", log_hex(mode));
        return swizzle_identity;
    }
}

MTLTextureSwizzleChannels translate_swizzle(SceGxmTextureFormat format) {
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
        return swizzle_identity;

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
    case SCE_GXM_TEXTURE_BASE_FORMAT_U2U10U10U10:
    case SCE_GXM_TEXTURE_BASE_FORMAT_U2F10F10F10:
    case SCE_GXM_TEXTURE_BASE_FORMAT_UBC1:
    case SCE_GXM_TEXTURE_BASE_FORMAT_UBC2:
    case SCE_GXM_TEXTURE_BASE_FORMAT_UBC3:
        return translate_swizzle4(static_cast<SceGxmTextureSwizzle4Mode>(swizzle));

    case SCE_GXM_TEXTURE_BASE_FORMAT_U4U4U4U4:
        return translate_swizzle4_abgr(static_cast<SceGxmTextureSwizzle4Mode>(swizzle));

    case SCE_GXM_TEXTURE_BASE_FORMAT_U1U5U5U5:
        return translate_swizzle4_a1r5g5b5(static_cast<SceGxmTextureSwizzle4Mode>(swizzle));

    // YUV formats are decoded to RGBA
    case SCE_GXM_TEXTURE_BASE_FORMAT_YUV420P2:
    case SCE_GXM_TEXTURE_BASE_FORMAT_YUV420P3:
    case SCE_GXM_TEXTURE_BASE_FORMAT_YUV422:
        return swizzle_identity;

    default:
        LOG_ERROR("Unknown format {}", log_hex(base_format));
        return swizzle_identity;
    }
}

MTLPixelFormat translate_format(SceGxmTextureBaseFormat base_format, bool support_packed16) {
    switch (base_format) {
    case SCE_GXM_TEXTURE_BASE_FORMAT_U8:
        return MTLPixelFormatR8Unorm;
    case SCE_GXM_TEXTURE_BASE_FORMAT_S8:
        return MTLPixelFormatR8Snorm;
    case SCE_GXM_TEXTURE_BASE_FORMAT_U16:
        return MTLPixelFormatR16Unorm;
    case SCE_GXM_TEXTURE_BASE_FORMAT_S16:
        return MTLPixelFormatR16Snorm;
    case SCE_GXM_TEXTURE_BASE_FORMAT_F16:
        return MTLPixelFormatR16Float;
    case SCE_GXM_TEXTURE_BASE_FORMAT_U32:
        return MTLPixelFormatR32Uint;
    case SCE_GXM_TEXTURE_BASE_FORMAT_S32:
        return MTLPixelFormatR32Sint;
    case SCE_GXM_TEXTURE_BASE_FORMAT_X8U24:
    case SCE_GXM_TEXTURE_BASE_FORMAT_F32:
    case SCE_GXM_TEXTURE_BASE_FORMAT_F32M:
        return MTLPixelFormatR32Float;

    case SCE_GXM_TEXTURE_BASE_FORMAT_U8U8:
        return MTLPixelFormatRG8Unorm;
    case SCE_GXM_TEXTURE_BASE_FORMAT_S8S8:
        return MTLPixelFormatRG8Snorm;
    case SCE_GXM_TEXTURE_BASE_FORMAT_U16U16:
        return MTLPixelFormatRG16Unorm;
    case SCE_GXM_TEXTURE_BASE_FORMAT_S16S16:
        return MTLPixelFormatRG16Snorm;
    case SCE_GXM_TEXTURE_BASE_FORMAT_F16F16:
        return MTLPixelFormatRG16Float;
    case SCE_GXM_TEXTURE_BASE_FORMAT_U32U32:
        return MTLPixelFormatRG32Uint;
    case SCE_GXM_TEXTURE_BASE_FORMAT_F32F32:
        return MTLPixelFormatRG32Float;

    case SCE_GXM_TEXTURE_BASE_FORMAT_U8U8U8U8:
        return MTLPixelFormatRGBA8Unorm;
    case SCE_GXM_TEXTURE_BASE_FORMAT_S8S8S8S8:
        return MTLPixelFormatRGBA8Snorm;
    case SCE_GXM_TEXTURE_BASE_FORMAT_U16U16U16U16:
        return MTLPixelFormatRGBA16Unorm;
    case SCE_GXM_TEXTURE_BASE_FORMAT_S16S16S16S16:
        return MTLPixelFormatRGBA16Snorm;
    case SCE_GXM_TEXTURE_BASE_FORMAT_F16F16F16F16:
        return MTLPixelFormatRGBA16Float;

    case SCE_GXM_TEXTURE_BASE_FORMAT_U5U6U5:
        return support_packed16 ? MTLPixelFormatB5G6R5Unorm : MTLPixelFormatRGBA8Unorm;
    case SCE_GXM_TEXTURE_BASE_FORMAT_F11F11F10:
        return MTLPixelFormatRG11B10Float;
    case SCE_GXM_TEXTURE_BASE_FORMAT_SE5M9M9M9:
        return MTLPixelFormatRGB9E5Float;

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
        return MTLPixelFormatRGBA8Unorm;

    // Because of the lack of support of s8s8s8, an alpha channel is added to this texture
    case SCE_GXM_TEXTURE_BASE_FORMAT_S8S8S8:
        return MTLPixelFormatRGBA8Snorm;

    case SCE_GXM_TEXTURE_BASE_FORMAT_U4U4U4U4:
        return support_packed16 ? MTLPixelFormatABGR4Unorm : MTLPixelFormatRGBA8Unorm;
    case SCE_GXM_TEXTURE_BASE_FORMAT_U1U5U5U5:
        return support_packed16 ? MTLPixelFormatBGR5A1Unorm : MTLPixelFormatRGBA8Unorm;
    case SCE_GXM_TEXTURE_BASE_FORMAT_U2U10U10U10:
        return MTLPixelFormatBGR10A2Unorm;
    case SCE_GXM_TEXTURE_BASE_FORMAT_U2F10F10F10:
        return MTLPixelFormatRGBA16Float;

    case SCE_GXM_TEXTURE_BASE_FORMAT_UBC1:
        return MTLPixelFormatBC1_RGBA;
    case SCE_GXM_TEXTURE_BASE_FORMAT_UBC2:
        return MTLPixelFormatBC2_RGBA;
    case SCE_GXM_TEXTURE_BASE_FORMAT_UBC3:
        return MTLPixelFormatBC3_RGBA;
    case SCE_GXM_TEXTURE_BASE_FORMAT_UBC4:
        return MTLPixelFormatBC4_RUnorm;
    case SCE_GXM_TEXTURE_BASE_FORMAT_SBC4:
        return MTLPixelFormatBC4_RSnorm;
    case SCE_GXM_TEXTURE_BASE_FORMAT_UBC5:
        return MTLPixelFormatBC5_RGUnorm;
    case SCE_GXM_TEXTURE_BASE_FORMAT_SBC5:
        return MTLPixelFormatBC5_RGSnorm;
    case SCE_GXM_TEXTURE_BASE_FORMAT_UBC6H:
        return MTLPixelFormatBC6H_RGBUfloat;
    case SCE_GXM_TEXTURE_BASE_FORMAT_SBC6H:
        return MTLPixelFormatBC6H_RGBFloat;
    case SCE_GXM_TEXTURE_BASE_FORMAT_UBC7:
        return MTLPixelFormatBC7_RGBAUnorm;

#define ASTC_FMT(b_x, b_y)                              \
    case SCE_GXM_TEXTURE_BASE_FORMAT_ASTC##b_x##x##b_y: \
        return MTLPixelFormatASTC_##b_x##x##b_y##_LDR;
#include "../texture/astc_formats.inc"
#undef ASTC_FMT

    default:
        LOG_ERROR("Unknown format {}", log_hex(base_format));
        return MTLPixelFormatRGBA8Unorm;
    }
}

MTLSamplerAddressMode translate_address_mode(SceGxmTextureAddrMode src) {
    switch (src) {
    case SCE_GXM_TEXTURE_ADDR_REPEAT:
        return MTLSamplerAddressModeRepeat;
    case SCE_GXM_TEXTURE_ADDR_MIRROR:
        return MTLSamplerAddressModeMirrorRepeat;
    case SCE_GXM_TEXTURE_ADDR_CLAMP:
        return MTLSamplerAddressModeClampToEdge;
    case SCE_GXM_TEXTURE_ADDR_MIRROR_CLAMP:
        return MTLSamplerAddressModeMirrorClampToEdge;
    case SCE_GXM_TEXTURE_ADDR_REPEAT_IGNORE_BORDER:
    case SCE_GXM_TEXTURE_ADDR_CLAMP_FULL_BORDER:
    case SCE_GXM_TEXTURE_ADDR_CLAMP_IGNORE_BORDER:
    case SCE_GXM_TEXTURE_ADDR_CLAMP_HALF_BORDER:
        LOG_ERROR_ONCE("Unhandled border color address mode, texture will be corrupted. Please report it to the developers.");
        return (src == SCE_GXM_TEXTURE_ADDR_REPEAT_IGNORE_BORDER) ? MTLSamplerAddressModeRepeat : MTLSamplerAddressModeClampToBorderColor;
    default:
        LOG_ERROR("Unknown address mode {}", log_hex(src));
        return MTLSamplerAddressModeClampToEdge;
    }
}

MTLSamplerMinMagFilter translate_filter(SceGxmTextureFilter src) {
    switch (src) {
    case SCE_GXM_TEXTURE_FILTER_POINT:
    case SCE_GXM_TEXTURE_FILTER_MIPMAP_POINT:
        return MTLSamplerMinMagFilterNearest;
    case SCE_GXM_TEXTURE_FILTER_LINEAR:
    case SCE_GXM_TEXTURE_FILTER_MIPMAP_LINEAR:
        return MTLSamplerMinMagFilterLinear;
    default:
        LOG_ERROR("Unknown texture filter {}", log_hex(src));
        return MTLSamplerMinMagFilterNearest;
    }
}
} // namespace texture
} // namespace renderer::metal
