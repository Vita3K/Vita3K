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

#include <vulkan/vulkan.hpp>

#include <bit>
#include <cstdint>

namespace renderer::vulkan::detail {

inline const vk::Pipeline pipeline_compiling = std::bit_cast<vk::Pipeline>(~uint64_t{ 0 });
inline const vk::Pipeline pipeline_failed = std::bit_cast<vk::Pipeline>(~uint64_t{ 1 });

inline bool is_usable_pipeline(vk::Pipeline pipeline) {
    return pipeline && pipeline != pipeline_compiling && pipeline != pipeline_failed;
}

inline vk::Pipeline completed_pipeline(vk::Pipeline pipeline) {
    return pipeline ? pipeline : pipeline_failed;
}

template <typename Dispatch>
vk::Result create_graphics_pipeline(vk::Device device, vk::PipelineCache cache,
    const vk::GraphicsPipelineCreateInfo &info, vk::Pipeline &pipeline, const Dispatch &dispatch) {
    pipeline = nullptr;
    return device.createGraphicsPipelines(cache, 1, &info, nullptr, &pipeline, dispatch);
}

} // namespace renderer::vulkan::detail
