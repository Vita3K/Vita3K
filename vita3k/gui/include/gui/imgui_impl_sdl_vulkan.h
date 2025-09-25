// dear imgui: Renderer Backend for Vulkan
// This needs to be used along with a Platform Backend (e.g. GLFW, SDL, Win32, custom..)

// Implemented features:
//  [X] Renderer: Support for large meshes (64k+ vertices) with 16-bit indices.
//  [!] Renderer: User texture binding. Use 'VkDescriptorSet' as ImTextureID. Read the FAQ about ImTextureID! See https://github.com/ocornut/imgui/pull/914 for discussions.

// Important: on 32-bit systems, user texture binding is only supported if your imconfig file has '#define ImTextureID ImU64'.
// See imgui_impl_vulkan.cpp file for details.

// You can use unmodified imgui_impl_* files in your project. See examples/ folder for examples of using this.
// Prefer including the entire imgui/ repository into your project (either as a copy or as a submodule), and only build the backends you need.
// If you are new to Dear ImGui, read documentation from the docs/ folder + read the top of imgui.cpp.
// Read online: https://github.com/ocornut/imgui/tree/master/docs

// The aim of imgui_impl_vulkan.h/.cpp is to be usable in your engine without any modification.
// IF YOU FEEL YOU NEED TO MAKE ANY CHANGE TO THIS CODE, please share them and your feedback at https://github.com/ocornut/imgui/

// Important note to the reader who wish to integrate imgui_impl_vulkan.cpp/.h in their own engine/app.
// - Common ImGui_ImplVulkan_XXX functions and structures are used to interface with imgui_impl_vulkan.cpp/.h.
//   You will use those if you want to use this rendering backend in your engine/app.
// - Helper ImGui_ImplVulkanH_XXX functions and structures are only used by this example (main.cpp) and by
//   the backend itself (imgui_impl_vulkan.cpp), but should PROBABLY NOT be used by your own engine/app code.
// Read comments in imgui_impl_vulkan.h.

#pragma once

#include <emuenv/window.h>
#include <gui/imgui_impl_sdl_state.h>
#include <renderer/vulkan/state.h>

typedef union SDL_Event SDL_Event;

// Reusable buffers used for rendering 1 current in-flight frame, for ImGui_ImplVulkan_RenderDrawData()
// [Please zero-clear before use!]
struct ImGui_ImplVulkanH_FrameRenderBuffers {
    vma::Allocation VertexBufferAllocation;
    vma::Allocation IndexBufferAllocation;
    vk::DeviceSize VertexBufferSize;
    vk::DeviceSize IndexBufferSize;
    vk::Buffer VertexBuffer;
    vk::Buffer IndexBuffer;
};

// Each viewport will hold 1 ImGui_ImplVulkanH_WindowRenderBuffers
// [Please zero-clear before use!]
struct ImGui_ImplVulkanH_WindowRenderBuffers {
    uint32_t Index;
    uint32_t Count;
    ImGui_ImplVulkanH_FrameRenderBuffers *FrameRenderBuffers = nullptr;
};

struct TextureState {
    static constexpr uint32_t nb_descriptor_sets = 1024;

    vma::Allocation allocation;
    vk::Image image;
    vk::ImageView image_view;
    vk::DescriptorSet descriptor_set;
    uint64_t last_frame_used = 0;
};

struct ImGui_VulkanState : public ImGui_State {
    vk::RenderPass RenderPass{};
    vk::DeviceSize BufferMemoryAlignment = 256;
    vk::PipelineCreateFlags PipelineCreateFlags{};
    vk::DescriptorSetLayout DescriptorSetLayout{};
    vk::PipelineLayout PipelineLayout{};
    vk::Pipeline Pipeline{};
    uint32_t Subpass{};
    vk::ShaderModule ShaderModuleVert{};
    vk::ShaderModule ShaderModuleFrag{};

    // Font data
    vk::Sampler FontSampler{};
    TextureState *Font;
    vma::Allocation UploadBufferAllocation{};
    vk::Buffer UploadBuffer{};
    vk::CommandBuffer CommandBuffer{};

    vk::DescriptorPool DescriptorPool;
    std::vector<vk::DescriptorSet> DescriptorSets;
    uint32_t DescriptorIdx = 0;

    uint64_t frame_timestamp = 1;

    // Render buffers
    ImGui_ImplVulkanH_WindowRenderBuffers MainWindowRenderBuffers;

    // Texture management
    vk::CommandPool TexCommandPool{};
    vk::CommandBuffer TexCommandBuffer{};
};

IMGUI_API ImGui_VulkanState *ImGui_ImplSdlVulkan_Init(renderer::State *renderer, SDL_Window *window);
IMGUI_API void ImGui_ImplSdlVulkan_Shutdown(ImGui_VulkanState &state);
IMGUI_API void ImGui_ImplSdlVulkan_RenderDrawData(ImGui_VulkanState &state);

// if is_alpha is set to true, the texture only has one alpha component, the other channels map to 1
IMGUI_API ImTextureID ImGui_ImplSdlVulkan_CreateTexture(ImGui_VulkanState &state, void *pixels, int width, int height, bool is_alpha = false);
IMGUI_API void ImGui_ImplSdlVulkan_DeleteTexture(ImGui_VulkanState &state, ImTextureID texture);

// (Advanced) Use e.g. if you need to precisely control the timing of texture updates (e.g. for staged rendering), by setting ImDrawData::Textures = NULL to handle this manually.
IMGUI_API void ImGui_ImplSdlVulkan_UpdateTexture(ImGui_VulkanState &state, ImTextureData *tex);

// Use if you want to reset your rendering device without losing ImGui state.
IMGUI_API void ImGui_ImplSdlVulkan_InvalidateDeviceObjects(ImGui_VulkanState &state);
IMGUI_API bool ImGui_ImplSdlVulkan_CreateDeviceObjects(ImGui_VulkanState &state);
