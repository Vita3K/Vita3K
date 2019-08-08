#include <gui/imgui_impl_sdl_vulkan.h>

#include <renderer/types.h>
#include <renderer/vulkan/state.h>
#include <renderer/vulkan/functions.h>

#include <SDL_vulkan.h>

#include <fstream>

// MB(1)
constexpr size_t gui_max_draw_size = 1 * 1024 * 1024;

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

    state.gui_vulkan.vertex_module = load_shader(state.device, base_path + "shaders-builtin/vulkan_imgui_vert.spv");
    state.gui_vulkan.fragment_module = load_shader(state.device, base_path + "shaders-builtin/vulkan_imgui_frag.spv");

    return true;
}

IMGUI_API void ImGui_ImplSdlVulkan_Shutdown(RendererPtr &renderer) {
    auto &state = vulkan_state(renderer);
}
IMGUI_API void ImGui_ImplSdlVulkan_RenderDrawData(RendererPtr &renderer, ImDrawData *draw_data) {
    auto &state = vulkan_state(renderer);

    state.gui_vulkan.command_buffer.reset(vk::CommandBufferResetFlags());

    vk::CommandBufferBeginInfo begin_info(
        vk::CommandBufferUsageFlags(), // No Flags
        nullptr // Inheritance
        );

    state.gui_vulkan.command_buffer.begin(begin_info);

    state.gui_vulkan.command_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, state.gui_vulkan.pipeline);

    for (uint32_t a = 0; a < draw_data->CmdListsCount; a++) {
        ImDrawList *draw_list = draw_data->CmdLists[a];

    }

    state.gui_vulkan.command_buffer.end();
    renderer::vulkan::submit_command_buffer(state, renderer::vulkan::CommandType::General, state.gui_vulkan.command_buffer);
}

IMGUI_API void ImGui_ImplSdlVulkan_GetDrawableSize(SDL_Window *window, int &width, int &height) {
    SDL_Vulkan_GetDrawableSize(window, &width, &height);
}

static void ImGui_ImplSdlVulkan_InvalidateFontsTexture(renderer::vulkan::VulkanState &state) {
    renderer::vulkan::free_image(state, state.gui_vulkan.font_texture, state.gui_vulkan.font_allocation);
}

static void ImGui_ImplSdlVulkan_CreateFontsTexture(renderer::vulkan::VulkanState &state) {
    ImGuiIO &io = ImGui::GetIO();
    uint8_t *pixels;
    int width, height;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);

    const size_t buffer_size = width * height * 4;

    vk::BufferCreateInfo buffer_info(
        vk::BufferCreateFlags(), // No Flags
        buffer_size, // Size
        vk::BufferUsageFlagBits::eTransferSrc, // Usage
        vk::SharingMode::eExclusive, 0, nullptr // Sharing Mode
        );

    VmaAllocation temp_allocation;
    vk::Buffer temp_buffer = renderer::vulkan::create_buffer(state, buffer_info, renderer::vulkan::MemoryType::Mappable, temp_allocation);

    uint8_t *temp_memory;
    vmaMapMemory(state.allocator, temp_allocation, reinterpret_cast<void **>(&temp_memory));
    assert(temp_memory);
    std::memcpy(temp_memory, pixels, buffer_size);
    vmaUnmapMemory(state.allocator, temp_allocation);
    vmaFlushAllocation(state.allocator, temp_allocation, 0, VK_WHOLE_SIZE);

    vk::ImageCreateInfo image_info(
        vk::ImageCreateFlags(), // No Flags
        vk::ImageType::e2D, // 2D Image
        vk::Format::eR8G8B8A8Uint, // RGBA32
        vk::Extent3D(width, height, 1), // Size
        1, // Mip Levels
        1, // Array Layers
        vk::SampleCountFlagBits::e1, // No Multisampling
        vk::ImageTiling::eOptimal, // Optimal Image Tiling
        vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled, // Image can be copied to and sampled.
        vk::SharingMode::eExclusive, // Only one queue family can access at a time.
        0, nullptr, // Ignored on Exclusive sharing mode
        vk::ImageLayout::eUndefined // Sampling Layout (must be undefined)
        );

    state.gui_vulkan.font_texture = renderer::vulkan::create_image(
        state, image_info, renderer::vulkan::MemoryType::Device, state.gui_vulkan.font_allocation);

    vk::CommandBuffer transfer_buffer = renderer::vulkan::create_command_buffer(state, renderer::vulkan::CommandType::Transfer);

    vk::CommandBufferBeginInfo begin_info(
        vk::CommandBufferUsageFlagBits::eOneTimeSubmit, // One Time Buffer
        nullptr // Inheritance Info
        );

    transfer_buffer.begin(begin_info);

    vk::ImageSubresourceRange font_range(
        vk::ImageAspectFlagBits::eColor, // Color
        0, 1, // First Level
        0, 1 // First Layer
        );

    vk::ImageMemoryBarrier image_transfer_optimal_barrier(
        vk::AccessFlags(), // Was not written yet.
        vk::AccessFlagBits::eTransferWrite, // Will be written by a transfer operation.
        vk::ImageLayout::eUndefined, // Old Layout
        vk::ImageLayout::eTransferDstOptimal, // New Layout
        state.transfer_family_index, state.transfer_family_index, // No Queue Family Transition
        state.gui_vulkan.font_texture,
        font_range // Subresource Range
        );

    transfer_buffer.pipelineBarrier(
        vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eTransfer, // Top Of Pipe -> Transfer Stage
        vk::DependencyFlags(), // No Dependency Flags
        0, nullptr, // No Memory Barriers
        0, nullptr, // No Buffer Memory Barriers
        1, &image_transfer_optimal_barrier // 1 Image Memory Barrier
        );

    vk::BufferImageCopy region(
        0, // Buffer Offset
        width, // Buffer Row Length
        height, // Buffer Height
        vk::ImageSubresourceLayers(
            vk::ImageAspectFlagBits::eColor, // Aspects
            0, 0, 1 // First Layer/Level
        ),
        vk::Offset3D(0, 0, 0), // Image Offset
        vk::Extent3D(width, height, 1) // Image Extent
        );

    transfer_buffer.copyBufferToImage(
        temp_buffer, // Buffer
        state.gui_vulkan.font_texture, // Image
        vk::ImageLayout::eTransferDstOptimal, // Image Layout
        1, &region // Regions
        );

    vk::ImageMemoryBarrier image_shader_read_only_barrier(
        vk::AccessFlags(), // Was not written yet.
        vk::AccessFlagBits::eShaderRead, // Will be read by the shader.
        vk::ImageLayout::eTransferDstOptimal, // Old Layout
        vk::ImageLayout::eShaderReadOnlyOptimal, // New Layout
        state.transfer_family_index, state.general_family_index, // No Queue Family Transition
        state.gui_vulkan.font_texture,
        font_range // Subresource Range
        );

    transfer_buffer.pipelineBarrier(
        vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eFragmentShader, // Transfer -> Fragment Shader Stage
        vk::DependencyFlags(), // No Dependency Flags
        0, nullptr, // No Memory Barriers
        0, nullptr, // No Buffer Barriers
        1, &image_shader_read_only_barrier // 1 Image Memory Barrier
        );

    transfer_buffer.end();

    renderer::vulkan::submit_command_buffer(state, renderer::vulkan::CommandType::Transfer, transfer_buffer, true);
    renderer::vulkan::free_command_buffer(state, renderer::vulkan::CommandType::Transfer, transfer_buffer);

    renderer::vulkan::free_buffer(state, temp_buffer, temp_allocation);

    io.Fonts->TexID = reinterpret_cast<ImTextureID>(static_cast<VkImage>(state.gui_vulkan.font_texture));
}

// Use if you want to reset your rendering device without losing ImGui state.
IMGUI_API void ImGui_ImplSdlVulkan_InvalidateDeviceObjects(RendererPtr &renderer) {
    auto &state = vulkan_state(renderer);

    renderer::vulkan::free_command_buffer(state, renderer::vulkan::CommandType::General, state.gui_vulkan.command_buffer);

    ImGui_ImplSdlVulkan_InvalidateFontsTexture(state);

    state.device.destroy(state.gui_vulkan.pipeline);
    state.device.destroy(state.gui_vulkan.pipeline_layout);
    state.device.destroy(state.gui_vulkan.descriptor_set_layout);

    state.device.destroy(state.gui_vulkan.sampler);
    state.device.destroy(state.gui_vulkan.framebuffer);
    state.device.destroy(state.gui_vulkan.renderpass);
}
IMGUI_API bool ImGui_ImplSdlVulkan_CreateDeviceObjects(RendererPtr &renderer) {
    auto &state = vulkan_state(renderer);

    // Create GUI Renderpass
    std::vector<vk::AttachmentDescription> attachments = {
        vk::AttachmentDescription(
            vk::AttachmentDescriptionFlags(), // No Flags
            vk::Format::eB8G8R8A8Unorm, // Format, MoltenVK requires bgra?
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

    state.gui_vulkan.renderpass = state.device.createRenderPass(renderpass_info, nullptr);
    assert(state.gui_vulkan.renderpass);

    // Create Framebuffer
    vk::FramebufferCreateInfo framebuffer_info(
        vk::FramebufferCreateFlags(), // No Flags
        state.gui_vulkan.renderpass, // Renderpass
        2 /*state.swapchain_views.size()*/, state.swapchain_views/*.data()*/, // Attachments
        DEFAULT_RES_WIDTH, DEFAULT_RES_HEIGHT, // Size
        1 // Layers
    );

    state.gui_vulkan.framebuffer = state.device.createFramebuffer(framebuffer_info, nullptr);

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

    state.gui_vulkan.sampler = state.device.createSampler(sampler_info, nullptr);
    assert(state.gui_vulkan.sampler);

    vk::DescriptorSetLayoutBinding texture_binding_info(
        0, // Binding
        vk::DescriptorType::eCombinedImageSampler, // Descriptor Type
        1, // Array Size
        vk::ShaderStageFlagBits::eFragment,
        &state.gui_vulkan.sampler
    );

    vk::DescriptorSetLayoutCreateInfo descriptor_info(
        vk::DescriptorSetLayoutCreateFlags(), // No Flags
        1, &texture_binding_info // Bindings
    );

    state.gui_vulkan.descriptor_set_layout = state.device.createDescriptorSetLayout(descriptor_info, nullptr);
    assert(state.gui_vulkan.descriptor_set_layout);

    vk::PushConstantRange matrix_push_constant(
        vk::ShaderStageFlagBits::eVertex,
        0, // Offset
        sizeof(float) * 4 * 4 // Size (mat4)
    );

    vk::PipelineLayoutCreateInfo pipeline_layout_info(
        vk::PipelineLayoutCreateFlags(), // No Flags
        1, &state.gui_vulkan.descriptor_set_layout, // Descriptor Layouts
        1, &matrix_push_constant // Push Constants
    );

    state.gui_vulkan.pipeline_layout = state.device.createPipelineLayout(pipeline_layout_info, nullptr);
    assert(state.gui_vulkan.pipeline_layout);

    std::vector<vk::PipelineShaderStageCreateInfo> shader_stage_infos = {
        vk::PipelineShaderStageCreateInfo(
            vk::PipelineShaderStageCreateFlags(), // No Flags
            vk::ShaderStageFlagBits::eVertex, // Vertex Shader
            state.gui_vulkan.vertex_module, // Module
            "main", // Name
            nullptr // Specialization
        ),
        vk::PipelineShaderStageCreateInfo(
            vk::PipelineShaderStageCreateFlags(), // No Flags
            vk::ShaderStageFlagBits::eFragment, // Fragment Shader
            state.gui_vulkan.fragment_module, // Module
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
            vk::BlendFactor::eOneMinusSrcAlpha, // Src Alpha
            vk::BlendFactor::eDstAlpha, // Dst Alpha
            vk::BlendOp::eAdd, // Alpha Blend Op
            vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA
        ),
        vk::PipelineColorBlendAttachmentState(
            true, // Enable Blending
            vk::BlendFactor::eSrcColor, // Src Color
            vk::BlendFactor::eDstColor, // Dst Color
            vk::BlendOp::eAdd, // Color Blend Op
            vk::BlendFactor::eOneMinusSrcAlpha, // Src Alpha
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
        state.gui_vulkan.pipeline_layout,
        state.gui_vulkan.renderpass,
        0,
        vk::Pipeline(),
        0
    );

    state.gui_vulkan.pipeline = state.device.createGraphicsPipeline(vk::PipelineCache(), gui_pipeline_info, nullptr);
    assert(state.gui_vulkan.pipeline);

    ImGui_ImplSdlVulkan_CreateFontsTexture(state);

    state.gui_vulkan.command_buffer = renderer::vulkan::create_command_buffer(state, renderer::vulkan::CommandType::General);

    return true;
}
