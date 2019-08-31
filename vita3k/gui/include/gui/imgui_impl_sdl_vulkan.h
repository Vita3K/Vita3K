// Temporary Vulkan ImGui Implementation

#pragma once

#include <imgui.h>

#include <host/window.h>
#include <renderer/vulkan/state.h>
#include <gui/imgui_impl_sdl_state.h>

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

    inline renderer::vulkan::VulkanState &get_renderer() {
        return dynamic_cast<renderer::vulkan::VulkanState &>(*renderer);
    }
};

IMGUI_API ImGui_VulkanState *ImGui_ImplSdlVulkan_Init(renderer::State *renderer, SDL_Window *window, const std::string &base_path);
IMGUI_API void ImGui_ImplSdlVulkan_Shutdown(ImGui_VulkanState &state);
IMGUI_API void ImGui_ImplSdlVulkan_RenderDrawData(ImGui_VulkanState &state);

IMGUI_API ImTextureID ImGui_ImplSdlVulkan_CreateTexture(ImGui_VulkanState &state, void *data, int width, int height);
IMGUI_API void ImGui_ImplSdlVulkan_DeleteTexture(ImGui_VulkanState &state, ImTextureID texture);

// Use if you want to reset your rendering device without losing ImGui state.
IMGUI_API void ImGui_ImplSdlVulkan_InvalidateDeviceObjects(ImGui_VulkanState &state);
IMGUI_API bool ImGui_ImplSdlVulkan_CreateDeviceObjects(ImGui_VulkanState &state);
