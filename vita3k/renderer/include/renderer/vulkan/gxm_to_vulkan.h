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

#pragma once

#ifdef __APPLE__
#define VK_ENABLE_BETA_EXTENSIONS
#endif
#define VK_NO_PROTOTYPES
#define VULKAN_HPP_NO_CONSTRUCTORS
#define VULKAN_HPP_NO_SPACESHIP_OPERATOR
#include <vulkan/vulkan.hpp>

#include <gxm/types.h>

namespace renderer::vulkan {

vk::Format translate_attribute_format(SceGxmAttributeFormat format, unsigned int component_count, bool is_integer, bool is_signed);
vk::BlendFactor translate_blend_factor(const SceGxmBlendFactor blend_factor);
vk::BlendOp translate_blend_func(const SceGxmBlendFunc blend_func);
vk::PrimitiveTopology translate_primitive(SceGxmPrimitiveType primitive);
vk::CompareOp translate_depth_func(SceGxmDepthFunc depth_func);
vk::PolygonMode translate_polygon_mode(SceGxmPolygonMode polygon_mode);
vk::CullModeFlags translate_cull_mode(SceGxmCullMode cull_mode);
vk::CompareOp translate_stencil_func(SceGxmStencilFunc stencil_func);
vk::StencilOp translate_stencil_op(SceGxmStencilOp stencil_op);

namespace color {
vk::Format translate_format(SceGxmColorBaseFormat base_format);
vk::ComponentMapping translate_swizzle(SceGxmColorFormat format);
} // namespace color

namespace texture {
vk::Format translate_format(SceGxmTextureBaseFormat base_format);
vk::ComponentMapping translate_swizzle(SceGxmTextureFormat format);
vk::SamplerAddressMode translate_address_mode(SceGxmTextureAddrMode src);
vk::Filter translate_filter(SceGxmTextureFilter src);
} // namespace texture

} // namespace renderer::vulkan
