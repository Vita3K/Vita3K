#include <gui/imgui_impl_sdl_vulkan.h>

#include <renderer/types.h>
#include <renderer/vulkan/state.h>

#include <SDL_vulkan.h>

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
            ),
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
            ),
    };

    std::vector<vk::AttachmentReference> color_attachment_references = {
        vk::AttachmentReference(
            0, // attachments[0]
            vk::ImageLayout::eColorAttachmentOptimal // Image Layout
            ),
        vk::AttachmentReference(
            1, // attachments[0]
            vk::ImageLayout::eColorAttachmentOptimal // Image Layout
            ),
    };

    std::vector<vk::SubpassDescription> subpasses = {
        vk::SubpassDescription(
            vk::SubpassDescriptionFlags(), // No Flags
            vk::PipelineBindPoint::eGraphics, // Type
            0, nullptr, // No Inputs
            color_attachment_references.size(), color_attachment_references.data(), // Color Attachment References
            nullptr, nullptr, // No Resolve or Depth/Stencil for now
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
        2 /*state.swapchain_views.size()*/, state.swapchain_views/*.data()*/, // Attachments
        DEFAULT_RES_WIDTH, DEFAULT_RES_HEIGHT, // Size
        1 // Layers
        );

    state.gui_framebuffer = state.device.createFramebuffer(framebuffer_info, nullptr);

    vk::SamplerCreateInfo sampler_info(
        vk::SamplerCreateFlags(), // No Flags
        vk::Filter::eLinear, // Mag Filter
        vk::Filter::eLinear, // Min Filter
        vk::SamplerMipmapMode::eLinear, // Mipmap Mode
        vk::SamplerAddressMode::eRepeat, // U Mode
        vk::SamplerAddressMode::eRepeat, // V Mode
        vk::SamplerAddressMode::eRepeat, // W Mode
        0, // LOD Bias
        false, 1.0f, // Disable Anisotropy
        false, vk::CompareOp::eAlways, // No Comparing
        0.0f, 1.0f, // Min/Max LOD
        vk::BorderColor::eFloatOpaqueWhite, // Border Color
        false // Sampler is Normalized [0.0 - 1.0]
        );

    state.gui_sampler = state.device.createSampler(sampler_info, nullptr);
    assert(state.gui_sampler);

    vk::DescriptorSetLayoutBinding texture_binding_info(
        0, // Binding
        vk::DescriptorType::eCombinedImageSampler, // Descriptor Type
        1, // Array Size
        vk::ShaderStageFlagBits::eFragment,
        &state.gui_sampler
        );

    vk::DescriptorSetLayoutCreateInfo descriptor_info(
        vk::DescriptorSetLayoutCreateFlags(), // No Flags
        1, &texture_binding_info // Bindings
        );

    state.gui_descriptor_set = state.device.createDescriptorSetLayout(descriptor_info, nullptr);
    assert(state.gui_descriptor_set);

    vk::PushConstantRange matrix_push_constant(
        vk::ShaderStageFlagBits::eVertex,
        0, // Offset
        sizeof(float) * 4 * 4 // Size (mat4)
        );

    vk::PipelineLayoutCreateInfo pipeline_layout_info(
        vk::PipelineLayoutCreateFlags(), // No Flags
        1, &state.gui_descriptor_set, // Descriptor Layouts
        1, &matrix_push_constant // Push Constants
        );

    state.gui_pipeline_layout = state.device.createPipelineLayout(pipeline_layout_info, nullptr);
    assert(state.gui_pipeline_layout);

    vk::ShaderModule vertex_module = load_shader(state.device, base_path + "shaders-builtin/vulkan_imgui_vert.spv");
    vk::ShaderModule fragment_module = load_shader(state.device, base_path + "shaders-builtin/vulkan_imgui_frag.spv");

    std::vector<vk::PipelineShaderStageCreateInfo> shader_stage_infos = {
        vk::PipelineShaderStageCreateInfo(
            vk::PipelineShaderStageCreateFlags(), // No Flags
            vk::ShaderStageFlagBits::eVertex, // Vertex Shader
            vertex_module, // Module
            "main", // Name
            nullptr // Specialization
            ),
        vk::PipelineShaderStageCreateInfo(
            vk::PipelineShaderStageCreateFlags(), // No Flags
            vk::ShaderStageFlagBits::eFragment, // Fragment Shader
            fragment_module, // Module
            "main", // Name
            nullptr // Specialization
            ),
    };

    std::vector<vk::VertexInputBindingDescription> gui_pipeline_bindings = {
        vk::VertexInputBindingDescription(
            0, // Binding
            sizeof(ImDrawVert), // Stride
            vk::VertexInputRate::eVertex
            ),
    };

    std::vector<vk::VertexInputAttributeDescription> gui_pipeline_attributes = {
        vk::VertexInputAttributeDescription(
            0, // Location
            0, // Binding
            vk::Format::eR32G32Sfloat,
            0 // Offset
            ),
        vk::VertexInputAttributeDescription(
            1, // Location
            0, // Binding
            vk::Format::eR32G32Sfloat,
            sizeof(ImVec2) // Offset
            ),
        vk::VertexInputAttributeDescription(
            2, // Location
            0, // Binding
            vk::Format::eR8G8B8A8Uint,
            sizeof(ImVec2) * 2 // Offset
            ),
    };

    vk::PipelineVertexInputStateCreateInfo gui_pipeline_vertex_info(
        vk::PipelineVertexInputStateCreateFlags(), // No Flags
        gui_pipeline_bindings.size(), gui_pipeline_bindings.data(), // Bindings
        gui_pipeline_attributes.size(), gui_pipeline_attributes.data() // Attributes
        );

    vk::PipelineInputAssemblyStateCreateInfo gui_pipeline_assembly_info(
        vk::PipelineInputAssemblyStateCreateFlags(), // No Flags
        vk::PrimitiveTopology::eTriangleList, // Topology
        false // No Primitive Restart?
        );

    vk::Viewport viewport(-1.0f, -1.0f, 1.0f, 1.0f, 0.0f, 1.0f);
    vk::Rect2D scissor(vk::Offset2D(-1, -1), vk::Extent2D(2, 2));

    vk::PipelineViewportStateCreateInfo gui_pipeline_viewport_info(
        vk::PipelineViewportStateCreateFlags(),
        1, &viewport, // Viewport
        1, &scissor // Scissor
        );

    vk::PipelineRasterizationStateCreateInfo gui_pipeline_rasterization_info(
        vk::PipelineRasterizationStateCreateFlags(), // No Flags
        false, // No Depth Clamping
        false, // Rasterization is NOT Disabled
        vk::PolygonMode::eFill, // Fill Polygons
        vk::CullModeFlags(), // No Culling
        vk::FrontFace::eCounterClockwise, // Counter Clockwise Face Forwards
        false, 0, 0, 0, // No Depth Bias
        1.0f // Line Width
        );

    vk::PipelineMultisampleStateCreateInfo gui_pipeline_multisample_info(
        vk::PipelineMultisampleStateCreateFlags(), // No Flags
        vk::SampleCountFlagBits::e1, // No Multisampling
        false, 0, // No Sample Shading
        nullptr, // No Sample Mask
        false, false // Alpha Stays the Same
        );

    vk::PipelineDepthStencilStateCreateInfo gui_pipeline_depth_stencil_info(
        vk::PipelineDepthStencilStateCreateFlags(), // No Flags
        false, false, vk::CompareOp::eAlways, // No Depth Test
        false, // No Depth Bounds Test
        false, vk::StencilOpState(), vk::StencilOpState(), // No Stencil Test
        0.0f, 1.0f // Depth Bounds
        );

    std::vector<vk::PipelineColorBlendAttachmentState> attachment_blending = {
        vk::PipelineColorBlendAttachmentState(
            true, // Enable Blending
            vk::BlendFactor::eSrcColor, // Src Color
            vk::BlendFactor::eDstColor, // Dst Color
            vk::BlendOp::eAdd, // Color Blend Op
            vk::BlendFactor::eSrcAlpha, // Src Alpha
            vk::BlendFactor::eDstAlpha, // Dst Alpha
            vk::BlendOp::eAdd, // Alpha Blend Op
            vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA
            ),
        vk::PipelineColorBlendAttachmentState(
            true, // Enable Blending
            vk::BlendFactor::eSrcColor, // Src Color
            vk::BlendFactor::eDstColor, // Dst Color
            vk::BlendOp::eAdd, // Color Blend Op
            vk::BlendFactor::eSrcAlpha, // Src Alpha
            vk::BlendFactor::eDstAlpha, // Dst Alpha
            vk::BlendOp::eAdd, // Alpha Blend Op
            vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA
            ),
    };

    vk::PipelineColorBlendStateCreateInfo gui_pipeline_blend_info(
        vk::PipelineColorBlendStateCreateFlags(), // No Flags
        false, vk::LogicOp::eClear, // No Logic Op
        attachment_blending.size(), attachment_blending.data(), // Blending Attachments
        { 1, 1, 1, 1 } // Blend Constants
        );

    std::vector<vk::DynamicState> dynamic_states = {
        // Nothing so far...
    };

    vk::PipelineDynamicStateCreateInfo gui_pipeline_dynamic_info(
        vk::PipelineDynamicStateCreateFlags(), // No Flags
        dynamic_states.size(), dynamic_states.data() // Dynamic States
        );

    vk::GraphicsPipelineCreateInfo gui_pipeline_info(
        vk::PipelineCreateFlags(),
        shader_stage_infos.size(), shader_stage_infos.data(),
        &gui_pipeline_vertex_info,
        &gui_pipeline_assembly_info,
        nullptr, // No Tessellation
        &gui_pipeline_viewport_info,
        &gui_pipeline_rasterization_info,
        &gui_pipeline_multisample_info,
        &gui_pipeline_depth_stencil_info,
        &gui_pipeline_blend_info,
        &gui_pipeline_dynamic_info,
        state.gui_pipeline_layout,
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
IMGUI_API void ImGui_ImplSdlVulkan_RenderDrawData(RendererPtr &renderer, ImDrawData *draw_data) {
    auto &state = vulkan_state(renderer);

}

IMGUI_API void ImGui_ImplSdlVulkan_GetDrawableSize(SDL_Window *window, int &width, int &height) {
    SDL_Vulkan_GetDrawableSize(window, &width, &height);
}

// Use if you want to reset your rendering device without losing ImGui state.
IMGUI_API void ImGui_ImplSdlVulkan_InvalidateDeviceObjects(RendererPtr &renderer) {

}
IMGUI_API bool ImGui_ImplSdlVulkan_CreateDeviceObjects(RendererPtr &renderer) {

}
