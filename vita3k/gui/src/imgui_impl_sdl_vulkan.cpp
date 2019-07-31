#include <gui/imgui_impl_sdl_vulkan.h>

#include <renderer/vulkan/state.h>

#include <fstream>

static renderer::vulkan::VulkanState &vulkan_state(RendererPtr &renderer) {
    return reinterpret_cast<renderer::vulkan::VulkanState &>(*renderer.get());
}

static vk::ShaderModule load_shader(vk::Device device, const std::string &path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    assert(file.good());
    std::vector<char> shader_data(file.tellg());
    file.seekg(0, std::ios::beg);

    file.read(reinterpret_cast<char *>(shader_data.data()), shader_data.size());
    file.close();

    vk::ShaderModuleCreateInfo shader_info(
        vk::ShaderModuleCreateFlags(),
        shader_data.size(),
        reinterpret_cast<uint32_t *>(shader_data.data())
        );

    vk::ShaderModule module = device.createShaderModule(shader_info, nullptr);
    assert(module);

    return module;
}

IMGUI_API bool ImGui_ImplSdlVulkan_Init(RendererPtr &renderer, SDL_Window *window, const std::string &base_path) {
    auto &state = vulkan_state(renderer);

    // Create GUI Renderpass
    std::vector<vk::AttachmentDescription> attachments = {
        vk::AttachmentDescription(
            vk::AttachmentDescriptionFlags(), // No Flags
            vk::Format::eB8G8R8A8Unorm, // Format, MoltenVK requires rgba?
            vk::SampleCountFlagBits::e1, // No Multisampling
            vk::AttachmentLoadOp::eClear, // Clear Image
            vk::AttachmentStoreOp::eStore, // Keep Image Data
            vk::AttachmentLoadOp::eDontCare, // No Stencils
            vk::AttachmentStoreOp::eDontCare, // No Stencils
            vk::ImageLayout::eUndefined, // Initial Layout
            vk::ImageLayout::eColorAttachmentOptimal // Final Layout
            )
    };

    const uint32_t color_attachment_index = 0;

    vk::AttachmentReference color_attachment_reference(
        color_attachment_index, // attachments[color_attachment_index]
        vk::ImageLayout::eColorAttachmentOptimal // Image Layout
        );

    std::vector<vk::SubpassDescription> subpasses = {
        vk::SubpassDescription(
            vk::SubpassDescriptionFlags(), // No Flags
            vk::PipelineBindPoint::eGraphics, // Type
            0, nullptr, // No Inputs
            1, &color_attachment_reference, // Color Attachment References
            nullptr, nullptr, // No Resolve or Depth/Stencil for now
//            1, &color_attachment_index // Preserve Color Attachment
            0, nullptr // Vulkan Book says you don't need this for a Color Attachment?
            )
    };

    vk::RenderPassCreateInfo renderpass_info(
        vk::RenderPassCreateFlags(), // No Flags
        attachments.size(), attachments.data(), // Attachments
        subpasses.size(), subpasses.data(), // Subpasses
        0, nullptr // Dependencies
        );

    state.gui_renderpass = state.device.createRenderPass(renderpass_info, nullptr);
    assert(state.gui_renderpass);

    // Create Framebuffer
    vk::FramebufferCreateInfo framebuffer_info(
        vk::FramebufferCreateFlags(), // No Flags
        state.gui_renderpass, // Renderpass
        state.swapchain_views.size(), state.swapchain_views.data(), // Attachments
        960, 540, // Size
        1 // Layers
        );

    state.gui_framebuffer = state.device.createFramebuffer(framebuffer_info, nullptr);

    vk::ShaderModule vertex_module = load_shader(state.device, base_path + "shaders-builtin/vulkan_imgui_vert.spv");
    vk::ShaderModule fragment_module = load_shader(state.device, base_path + "shaders-builtin/vulkan_imgui_frag.spv");

    std::vector<vk::PipelineShaderStageCreateInfo> shader_stage_infos = {
        vk::PipelineShaderStageCreateInfo(
            vk::PipelineShaderStageCreateFlags(), // No Flags
            vk::ShaderStageFlagBits::eVertex, // Vertex Shader
            vertex_module, // Module
            "ImGui Vertex Shader", // Name
            nullptr // Specialization
            ),
        vk::PipelineShaderStageCreateInfo(
            vk::PipelineShaderStageCreateFlags(), // No Flags
            vk::ShaderStageFlagBits::eFragment, // Fragment Shader
            fragment_module, // Module
            "ImGui Fragment Shader", // Name
            nullptr // Specialization
            )
    };

    vk::GraphicsPipelineCreateInfo gui_pipeline_info(
        vk::PipelineCreateFlags(),
        shader_stage_infos.size(), shader_stage_infos.data(),
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        vk::PipelineLayout(),
        state.gui_renderpass,
        0,
        vk::Pipeline(),
        0
        );

    state.gui_pipeline = state.device.createGraphicsPipeline(vk::PipelineCache(), gui_pipeline_info, nullptr);
    assert(state.gui_pipeline);

    state.device.destroy(vertex_module);
    state.device.destroy(fragment_module);

    return true;
}

IMGUI_API void ImGui_ImplSdlVulkan_Shutdown(RendererPtr &renderer) {
    auto &state = vulkan_state(renderer);

    state.device.destroy(state.gui_renderpass);
}
IMGUI_API void ImGui_ImplSdlVulkan_NewFrame(RendererPtr &renderer, SDL_Window *window) {
    auto &state = vulkan_state(renderer);

}
IMGUI_API void ImGui_ImplSdlVulkan_RenderDrawData(RendererPtr &renderer, ImDrawData *draw_data) {
    auto &state = vulkan_state(renderer);

}
IMGUI_API bool ImGui_ImplSdlVulkan_ProcessEvent(RendererPtr &renderer, SDL_Event *event) {
    auto &state = vulkan_state(renderer);

}

// Use if you want to reset your rendering device without losing ImGui state.
IMGUI_API void ImGui_ImplSdlVulkan_InvalidateDeviceObjects(RendererPtr &renderer) {

}
IMGUI_API bool ImGui_ImplSdlVulkan_CreateDeviceObjects(RendererPtr &renderer) {

}
