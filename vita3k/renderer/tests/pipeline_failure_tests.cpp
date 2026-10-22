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

#include "vulkan/pipeline_cache_entry.h"

#include <gtest/gtest.h>

namespace {
VkResult next_result;
unsigned calls;

VKAPI_ATTR VkResult VKAPI_CALL create_pipeline(VkDevice, VkPipelineCache, uint32_t count,
    const VkGraphicsPipelineCreateInfo *, const VkAllocationCallbacks *, VkPipeline *pipelines) {
    ++calls;
    EXPECT_EQ(count, 1u);
    *pipelines = next_result == VK_SUCCESS ? std::bit_cast<VkPipeline>(uint64_t{ 0x1234 }) : VK_NULL_HANDLE;
    return next_result;
}

struct Dispatch {
    PFN_vkCreateGraphicsPipelines vkCreateGraphicsPipelines = create_pipeline;
    uint32_t getVkHeaderVersion() const { return VK_HEADER_VERSION; }
};
} // namespace

TEST(vulkan_pipeline, returns_driver_failures_without_throwing) {
    for (VkResult result : { VK_SUCCESS, VK_ERROR_OUT_OF_HOST_MEMORY, VK_ERROR_OUT_OF_DEVICE_MEMORY,
             VK_ERROR_UNKNOWN, VK_PIPELINE_COMPILE_REQUIRED_EXT }) {
        next_result = result;
        calls = 0;
        vk::Pipeline pipeline = std::bit_cast<vk::Pipeline>(uint64_t{ 0x4321 });
        vk::Result returned{};
        const vk::Device device = std::bit_cast<vk::Device>(uintptr_t{ 0x10 });
        const vk::GraphicsPipelineCreateInfo info{};
        EXPECT_NO_THROW(returned = renderer::vulkan::detail::create_graphics_pipeline(device, {}, info, pipeline, Dispatch{}));
        EXPECT_EQ(static_cast<VkResult>(returned), result);
        EXPECT_EQ(calls, 1u);
        EXPECT_EQ(bool(pipeline), result == VK_SUCCESS);
    }
}

#ifndef VULKAN_HPP_NO_EXCEPTIONS
TEST(vulkan_pipeline, enhanced_wrapper_throws_before_returning_an_error) {
    next_result = VK_ERROR_OUT_OF_HOST_MEMORY;
    calls = 0;
    const vk::Device device = std::bit_cast<vk::Device>(uintptr_t{ 0x10 });
    const vk::GraphicsPipelineCreateInfo info{};
    EXPECT_THROW((void)device.createGraphicsPipeline({}, info, nullptr, Dispatch{}), vk::SystemError);
    EXPECT_EQ(calls, 1u);
}
#endif

TEST(vulkan_pipeline, completed_failure_is_distinct_from_compiling) {
    using namespace renderer::vulkan::detail;
    const vk::Pipeline valid = std::bit_cast<vk::Pipeline>(uint64_t{ 0x1234 });
    EXPECT_EQ(completed_pipeline(valid), valid);
    EXPECT_EQ(completed_pipeline({}), pipeline_failed);
    EXPECT_NE(pipeline_failed, pipeline_compiling);
    EXPECT_FALSE(is_usable_pipeline({}));
    EXPECT_FALSE(is_usable_pipeline(pipeline_compiling));
    EXPECT_FALSE(is_usable_pipeline(pipeline_failed));
    EXPECT_TRUE(is_usable_pipeline(valid));
}
