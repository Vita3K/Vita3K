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

#pragma once

#include "metal_include.h"

#include <gxm/types.h>

#include <cstdint>

namespace renderer::metal {

// returns MTLVertexFormatInvalid when there is no matching format
MTLVertexFormat translate_attribute_format(SceGxmAttributeFormat format, unsigned int component_count, bool is_integer, bool is_signed);
MTLBlendFactor translate_blend_factor(const SceGxmBlendFactor blend_factor);
MTLBlendOperation translate_blend_func(const SceGxmBlendFunc blend_func);
// triangle fans are converted to triangle lists by the caller
MTLPrimitiveType translate_primitive(SceGxmPrimitiveType primitive);
MTLCompareFunction translate_depth_func(SceGxmDepthFunc depth_func);
MTLTriangleFillMode translate_polygon_mode(SceGxmPolygonMode polygon_mode);
MTLCullMode translate_cull_mode(SceGxmCullMode cull_mode);
MTLCompareFunction translate_stencil_func(SceGxmStencilFunc stencil_func);
MTLStencilOperation translate_stencil_op(SceGxmStencilOp stencil_op);

bool swizzle_equal(const MTLTextureSwizzleChannels &a, const MTLTextureSwizzleChannels &b);
bool swizzle_is_identity(const MTLTextureSwizzleChannels &swizzle);
// same as vkutil::color_to_texture_swizzle: combine the swizzle used to write a surface
// with the swizzle used to sample it
MTLTextureSwizzleChannels color_to_texture_swizzle(const MTLTextureSwizzleChannels &swizzle_color, const MTLTextureSwizzleChannels &swizzle_texture);

constexpr MTLTextureSwizzleChannels identity_swizzle = { MTLTextureSwizzleRed, MTLTextureSwizzleGreen, MTLTextureSwizzleBlue, MTLTextureSwizzleAlpha };

uint32_t bytes_per_pixel(MTLPixelFormat format);
uint32_t component_count(MTLPixelFormat format);
MTLPixelFormat linear_to_srgb(MTLPixelFormat format);
bool is_srgb_format(MTLPixelFormat format);
bool is_depth_stencil_format(MTLPixelFormat format);
const char *format_name(MTLPixelFormat format);

namespace color {
MTLPixelFormat translate_format(SceGxmColorBaseFormat base_format, bool support_packed16);
MTLTextureSwizzleChannels translate_swizzle(SceGxmColorFormat format);
} // namespace color

namespace texture {
MTLPixelFormat translate_format(SceGxmTextureBaseFormat base_format, bool support_packed16);
MTLTextureSwizzleChannels translate_swizzle(SceGxmTextureFormat format);
MTLSamplerAddressMode translate_address_mode(SceGxmTextureAddrMode src);
MTLSamplerMinMagFilter translate_filter(SceGxmTextureFilter src);
} // namespace texture

} // namespace renderer::metal
