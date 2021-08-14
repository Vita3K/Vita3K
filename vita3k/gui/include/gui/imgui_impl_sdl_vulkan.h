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

// Temporary Vulkan ImGui Implementation

#pragma once

#include <imgui.h>

#include <gui/imgui_impl_sdl_state.h>
#include <host/window.h>
#include <renderer/vulkan/state.h>

typedef union SDL_Event SDL_Event;

struct ImGui_VulkanState : public ImGui_State {
    vk::ShaderModule vertex_module;
    vk::ShaderModule fragment_module;

    vk::RenderPass renderpass;
    vk::Framebuffer framebuffers[2];
    vk::DescriptorSetLayout matrix_layout;
    vk::DescriptorSetLayout sampler_layout;
    vk::DescriptorPool descriptor_pool;
    vk::DescriptorSet matrix_set;
    vk::PipelineLayout pipeline_layout;
    vk::Pipeline pipeline;

    vk::Sampler sampler;
    ImTextureID font_texture{};
    VmaAllocation draw_allocation = VK_NULL_HANDLE;
    vk::Buffer draw_buffer;
    size_t draw_buffer_vertices = 0;
    VmaAllocation index_allocation = VK_NULL_HANDLE;
    vk::Buffer index_buffer;
    size_t index_buffer_indices = 0;
    VmaAllocation transformation_allocation = VK_NULL_HANDLE;
    vk::Buffer transformation_buffer;

    vk::CommandBuffer command_buffer;

    vk::Semaphore image_acquired_semaphore;
    vk::Semaphore render_complete_semaphore;
};

IMGUI_API ImGui_VulkanState *ImGui_ImplSdlVulkan_Init(renderer::State *renderer, SDL_Window *window, const std::string &base_path);
IMGUI_API void ImGui_ImplSdlVulkan_Shutdown(ImGui_VulkanState &state);
IMGUI_API void ImGui_ImplSdlVulkan_RenderDrawData(ImGui_VulkanState &state);

IMGUI_API ImTextureID ImGui_ImplSdlVulkan_CreateTexture(ImGui_VulkanState &state, void *data, int width, int height);
IMGUI_API void ImGui_ImplSdlVulkan_DeleteTexture(ImGui_VulkanState &state, ImTextureID texture);

// Use if you want to reset your rendering device without losing ImGui state.
IMGUI_API void ImGui_ImplSdlVulkan_InvalidateDeviceObjects(ImGui_VulkanState &state);
IMGUI_API bool ImGui_ImplSdlVulkan_CreateDeviceObjects(ImGui_VulkanState &state);
