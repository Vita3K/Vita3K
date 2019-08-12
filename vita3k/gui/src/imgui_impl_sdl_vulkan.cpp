#include <gui/imgui_impl_sdl_vulkan.h>

#include <renderer/types.h>
#include <renderer/vulkan/functions.h>
#include <renderer/vulkan/state.h>

#include <util/log.h>

#include <SDL_vulkan.h>

#include <fstream>

// 200000000 Nanoseconds = 0.2 seconds
constexpr uint64_t next_image_timeout = 200000000;

const std::array<float, 4> clear_color = { 0.125490203f, 0.698039234f, 0.666666687f, 1.0f };
const std::array<vk::ClearValue, 2> clear_values = {
    vk::ClearColorValue(clear_color),
    vk::ClearColorValue(clear_color),
};

// This is seperated because I use similar objects a lot and it is getting irritating to type.
const vk::ImageSubresourceRange base_subresource_range = vk::ImageSubresourceRange(
    vk::ImageAspectFlagBits::eColor, // Aspect
    0, 1, // Level Range
    0, 1 // Layer Range
);

const vk::IndexType imgui_index_type = sizeof(ImDrawIdx) == 2 ? vk::IndexType::eUint16 : vk::IndexType::eUint32;

static renderer::vulkan::VulkanState &vulkan_state(RendererPtr &renderer) {
    return reinterpret_cast<renderer::vulkan::VulkanState &>(*renderer.get());
}

static vk::ShaderModule load_shader(vk::Device device, const std::string &path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.good()) {
        LOG_ERROR("Could not find shader SPIR-V at `{}`.", path);
        return vk::ShaderModule();
    }
    std::vector<char> shader_data(file.tellg());
    file.seekg(0, std::ios::beg);

    file.read(shader_data.data(), shader_data.size());
    file.close();

    vk::ShaderModuleCreateInfo shader_info(
        vk::ShaderModuleCreateFlags(), // No Flags
        shader_data.size(), reinterpret_cast<uint32_t *>(shader_data.data()) // Code
    );

    vk::ShaderModule module = device.createShaderModule(shader_info, nullptr);
    if (!module)
        LOG_ERROR("Could not build Vulkan shader module from SPIR-V at `{}`.", path);

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

// Only one mapping can be created on a section of memory at a time. This method is split into the "vertex" and "index" parts to avoid overlapping maps.
static void ImGui_ImplSdlVulkan_UpdateBuffers(renderer::vulkan::VulkanState &state, ImDrawData *draw_data) {
    VkResult result;

    ImDrawVert *draw_buffer_data = nullptr;

    if (state.gui_vulkan.draw_buffer_vertices != draw_data->TotalVtxCount) {
        // Recreate Buffer
        if (state.gui_vulkan.draw_buffer)
            renderer::vulkan::destroy_buffer(state, state.gui_vulkan.draw_buffer, state.gui_vulkan.draw_allocation);

        vk::BufferCreateInfo draw_buffer_info(
            vk::BufferCreateFlags(), // No Flags
            draw_data->TotalVtxCount * sizeof(ImDrawVert), // Size
            vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eVertexBuffer, // Usage
            vk::SharingMode::eExclusive, 0, nullptr // Exclusive Sharing Mode
        );

        state.gui_vulkan.draw_buffer = renderer::vulkan::create_buffer(state, draw_buffer_info,
            renderer::vulkan::MemoryType::Mappable, state.gui_vulkan.draw_allocation);

        state.gui_vulkan.draw_buffer_vertices = draw_data->TotalVtxCount;
    }

    result = vmaMapMemory(state.allocator, state.gui_vulkan.draw_allocation,
        reinterpret_cast<void **>(&draw_buffer_data));
    if (result != VK_SUCCESS || !draw_buffer_data) {
        LOG_WARN("Failed to map memory for gui draw vertex buffer.");
        return;
    }

    size_t draw_buffer_pointer = 0;

    for (uint32_t a = 0; a < draw_data->CmdListsCount; a++) {
        ImDrawList *draw_list = draw_data->CmdLists[a];

        std::memcpy(&draw_buffer_data[draw_buffer_pointer], draw_list->VtxBuffer.Data, draw_list->VtxBuffer.Size * sizeof(ImDrawVert));

        draw_buffer_pointer += draw_list->VtxBuffer.Size;
    }

//    for (uint32_t vertex_id = 0; vertex_id < draw_data->TotalVtxCount; vertex_id++) {
//        ImDrawVert vert = draw_buffer_data[vertex_id];
//
//        LOG_INFO("Vertex {}: ({}, {}) uv ({}, {}) col {}", vertex_id, vert.pos.x, vert.pos.y, vert.uv.x, vert.uv.y, vert.col);
//    }

    vmaUnmapMemory(state.allocator, state.gui_vulkan.draw_allocation);

    // Write to index buffer...
    ImDrawIdx *index_buffer_data = nullptr;

    if (state.gui_vulkan.index_buffer_indices != draw_data->TotalIdxCount) {
        // Recreate Buffer
        if (state.gui_vulkan.index_buffer)
            renderer::vulkan::destroy_buffer(state, state.gui_vulkan.index_buffer, state.gui_vulkan.index_allocation);

        vk::BufferCreateInfo index_buffer_info(
            vk::BufferCreateFlags(), // No Flags
            draw_data->TotalIdxCount * sizeof(ImDrawIdx), // Size
            vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eIndexBuffer, // Usage
            vk::SharingMode::eExclusive, 0, nullptr // Exclusive Sharing Mode
        );

        state.gui_vulkan.index_buffer = renderer::vulkan::create_buffer(state, index_buffer_info,
            renderer::vulkan::MemoryType::Mappable, state.gui_vulkan.index_allocation);

        state.gui_vulkan.index_buffer_indices = draw_data->TotalIdxCount;
    }

    result = vmaMapMemory(state.allocator, state.gui_vulkan.index_allocation,
        reinterpret_cast<void **>(&index_buffer_data));
    if (result != VK_SUCCESS || !index_buffer_data) {
        LOG_WARN("Failed to map memory for gui index buffer.");
        return;
    }

    size_t index_buffer_pointer = 0;

    for (uint32_t a = 0; a < draw_data->CmdListsCount; a++) {
        ImDrawList *draw_list = draw_data->CmdLists[a];

        std::memcpy(&index_buffer_data[index_buffer_pointer], draw_list->IdxBuffer.Data, draw_list->IdxBuffer.Size * sizeof(ImDrawIdx));

        index_buffer_pointer += draw_list->IdxBuffer.Size;
    }

    vmaUnmapMemory(state.allocator, state.gui_vulkan.index_allocation);
}

IMGUI_API void ImGui_ImplSdlVulkan_RenderDrawData(RendererPtr &renderer, ImDrawData *draw_data) {
    auto &state = vulkan_state(renderer);

    uint32_t image_index = ~0u;
    state.device.resetFences(1, &state.gui_vulkan.next_image_fence);
    vk::Result acquire_result = state.device.acquireNextImageKHR(state.swapchain, next_image_timeout,
        vk::Semaphore(), state.gui_vulkan.next_image_fence, &image_index);

    // TODO: Use semaphores so acquiring can happen at the same time as the buffer? Better sync solution please.
    vk::Result wait_result = state.device.waitForFences(1, &state.gui_vulkan.next_image_fence,
        true, next_image_timeout);
    if (acquire_result == vk::Result::eTimeout || wait_result == vk::Result::eTimeout) {
        LOG_WARN("Missed timeout for acquiring next image.");
        return;
    }

    ImGui_ImplSdlVulkan_UpdateBuffers(state, draw_data);

    state.gui_vulkan.command_buffer.reset(vk::CommandBufferResetFlags());

    vk::CommandBufferBeginInfo begin_info(
        vk::CommandBufferUsageFlags(), // No Flags
        nullptr // Inheritance
    );

    state.gui_vulkan.command_buffer.begin(begin_info);

    // Identity for now...
    float matrix[] = {
        1.0f / DEFAULT_RES_WIDTH, 0, 0, 0,
        0, 1.0f / DEFAULT_RES_HEIGHT, 0, 0,
        0, 0, 1, 0,
        0, 0, 0, 1,
    };

    state.gui_vulkan.command_buffer.updateBuffer(state.gui_vulkan.transformation_buffer, 0, sizeof(matrix), matrix);

    vk::RenderPassBeginInfo renderpass_begin_info(
        state.gui_vulkan.renderpass, // Renderpass
        state.gui_vulkan.framebuffer, // Framebuffer
        vk::Rect2D(
            vk::Offset2D(0, 0),
            vk::Extent2D(DEFAULT_RES_WIDTH, DEFAULT_RES_HEIGHT)), // Render Area
        clear_values.size(), clear_values.data() // Clear Colors
    );

    state.gui_vulkan.command_buffer.beginRenderPass(renderpass_begin_info, vk::SubpassContents::eInline);
    state.gui_vulkan.command_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, state.gui_vulkan.pipeline);
    state.gui_vulkan.command_buffer.bindDescriptorSets(
        vk::PipelineBindPoint::eGraphics, // Bind Point
        state.gui_vulkan.pipeline_layout, // Layout
        0, // First Set
        1, &state.gui_vulkan.descriptor_set, // Sets
        0, nullptr // Dynamic Offsets
        );

    uint64_t vertex_offset = 0;
    uint64_t index_offset = 0;

    for (uint32_t a = 0; a < draw_data->CmdListsCount; a++) {
        ImDrawList *draw_list = draw_data->CmdLists[a];

        state.gui_vulkan.command_buffer.bindVertexBuffers(0, 1, &state.gui_vulkan.draw_buffer, &vertex_offset);
        state.gui_vulkan.command_buffer.bindIndexBuffer(state.gui_vulkan.index_buffer, index_offset, imgui_index_type);
        vertex_offset += draw_list->VtxBuffer.Size * sizeof(ImDrawVert);
        index_offset += draw_list->IdxBuffer.Size * sizeof(ImDrawIdx);

        uint64_t index_element_offset = 0;

        for (const auto &cmd : draw_list->CmdBuffer) {
            if (cmd.UserCallback) {
                cmd.UserCallback(draw_list, &cmd);
            } else {
                state.gui_vulkan.command_buffer.drawIndexed(cmd.ElemCount, 1, index_element_offset, 0, 0);
            }
        }
    }

    state.gui_vulkan.command_buffer.endRenderPass();

    vk::ImageMemoryBarrier color_attachment_present_barrier(
        vk::AccessFlagBits::eColorAttachmentWrite, // From rendering
        vk::AccessFlagBits::eMemoryRead, // To readable format for presenting
        vk::ImageLayout::eColorAttachmentOptimal,
        vk::ImageLayout::ePresentSrcKHR,
        state.general_family_index, state.general_family_index, // No Transfer
        state.swapchain_images[image_index], // Image
        base_subresource_range
        );

    state.gui_vulkan.command_buffer.pipelineBarrier(
        vk::PipelineStageFlagBits::eColorAttachmentOutput, vk::PipelineStageFlagBits::eBottomOfPipe, // Color Attachment Output -> Bottom of Pipe Stage
        vk::DependencyFlags(), // No Dependency Flags
        0, nullptr, // No Memory Barriers
        0, nullptr, // No Buffer Barriers
        1, &color_attachment_present_barrier // Image Barrier
        );

    state.gui_vulkan.command_buffer.end();

    // This is wait idle to ensure presentation happens after render. The command buffer should instead signal a fence or something.
    renderer::vulkan::submit_command_buffer(state, renderer::vulkan::CommandType::General, state.gui_vulkan.command_buffer, true);

    renderer::vulkan::present(state, image_index);
}

IMGUI_API void ImGui_ImplSdlVulkan_GetDrawableSize(SDL_Window *window, int &width, int &height) {
    SDL_Vulkan_GetDrawableSize(window, &width, &height);
}

static void ImGui_ImplSdlVulkan_InvalidateFontsTexture(renderer::vulkan::VulkanState &state) {
    renderer::vulkan::destroy_image(state, state.gui_vulkan.font_texture, state.gui_vulkan.font_allocation);
}

static bool ImGui_ImplSdlVulkan_CreateFontsTexture(renderer::vulkan::VulkanState &state) {
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
    VkResult result = vmaMapMemory(state.allocator, temp_allocation, reinterpret_cast<void **>(&temp_memory));
    if (result != VK_SUCCESS) {
        LOG_ERROR("Could not map font buffer memory. VMA result: {}.", result);
        return false;
    }
    if (!temp_memory) {
        LOG_ERROR("Could not map font buffer memory.");
        return false;
    }
    std::memcpy(temp_memory, pixels, buffer_size);
    vmaUnmapMemory(state.allocator, temp_allocation);

    vk::ImageCreateInfo image_info(
        vk::ImageCreateFlags(), // No Flags
        vk::ImageType::e2D, // 2D Image
        vk::Format::eR8G8B8A8Unorm, // RGBA32
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

    vk::ImageMemoryBarrier image_transfer_optimal_barrier(
        vk::AccessFlags(), // Was not written yet.
        vk::AccessFlagBits::eTransferWrite, // Will be written by a transfer operation.
        vk::ImageLayout::eUndefined, // Old Layout
        vk::ImageLayout::eTransferDstOptimal, // New Layout
        VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, // No Queue Family Transition
        state.gui_vulkan.font_texture,
        base_subresource_range // Subresource Range
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
        vk::AccessFlagBits::eTransferWrite, // Was just written to.
        vk::AccessFlagBits::eShaderRead, // Will be read by the shader.
        vk::ImageLayout::eTransferDstOptimal, // Old Layout
        vk::ImageLayout::eShaderReadOnlyOptimal, // New Layout
        VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, // No Queue Family Transition
        state.gui_vulkan.font_texture, // Image
        base_subresource_range // Subresource Range
    );

    transfer_buffer.pipelineBarrier(
        vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eFragmentShader, // Transfer -> Fragment Shader Stage
        vk::DependencyFlags(), // No Dependency Flags
        0, nullptr, // No Memory Barriers
        0, nullptr, // No Buffer Barriers
        1, &image_shader_read_only_barrier // Image Memory Barriers
    );

    transfer_buffer.end();

    renderer::vulkan::submit_command_buffer(state, renderer::vulkan::CommandType::Transfer, transfer_buffer, true);
    renderer::vulkan::free_command_buffer(state, renderer::vulkan::CommandType::Transfer, transfer_buffer);

    renderer::vulkan::destroy_buffer(state, temp_buffer, temp_allocation);

    vk::ImageViewCreateInfo font_view_info(
        vk::ImageViewCreateFlags(), // No Flags
        state.gui_vulkan.font_texture, // Image
        vk::ImageViewType::e2D, // Image View Type
        vk::Format::eR8G8B8A8Unorm, // Image View Format
        vk::ComponentMapping(), // Default Component Mapping (RGBA)
        base_subresource_range // Subresource Range
        );

    state.gui_vulkan.font_texture_view = state.device.createImageView(font_view_info);

    io.Fonts->TexID = reinterpret_cast<ImTextureID>(static_cast<VkImageView>(state.gui_vulkan.font_texture_view));

    return true;
}

// Use if you want to reset your rendering device without losing ImGui state.
IMGUI_API void ImGui_ImplSdlVulkan_InvalidateDeviceObjects(RendererPtr &renderer) {
    auto &state = vulkan_state(renderer);

    state.device.destroy(state.gui_vulkan.next_image_fence);

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
    if (!state.gui_vulkan.renderpass) {
        LOG_ERROR("Failed to create Vulkan gui renderpass.");
        return false;
    }

    // Create Framebuffer
    vk::FramebufferCreateInfo framebuffer_info(
        vk::FramebufferCreateFlags(), // No Flags
        state.gui_vulkan.renderpass, // Renderpass
        2 /*state.swapchain_views.size()*/, state.swapchain_views /*.data()*/, // Attachments
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
    if (!state.gui_vulkan.sampler) {
        LOG_ERROR("Failed to create Vulkan gui sampler.");
        return false;
    }

    std::vector<vk::DescriptorSetLayoutBinding> descriptor_layout_bindings = {
        vk::DescriptorSetLayoutBinding(
            0, // Binding
            vk::DescriptorType::eUniformBuffer, // Descriptor Type
            1, // Array Size
            vk::ShaderStageFlagBits::eVertex, // Usage Stage
            nullptr // Used Samplers
            ),
        vk::DescriptorSetLayoutBinding(
            1, // Binding
            vk::DescriptorType::eCombinedImageSampler, // Descriptor Type
            1, // Array Size
            vk::ShaderStageFlagBits::eFragment, // Usage Stage
            &state.gui_vulkan.sampler // Used Samplers
            ),
    };

    vk::DescriptorSetLayoutCreateInfo descriptor_layout_info(
        vk::DescriptorSetLayoutCreateFlags(), // No Flags
        descriptor_layout_bindings.size(), descriptor_layout_bindings.data() // Bindings
    );

    state.gui_vulkan.descriptor_set_layout = state.device.createDescriptorSetLayout(descriptor_layout_info, nullptr);
    if (!state.gui_vulkan.descriptor_set_layout) {
        LOG_ERROR("Failed to create Vulkan gui descriptor layout.");
        return false;
    }

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
    if (!state.gui_vulkan.pipeline_layout) {
        LOG_ERROR("Failed to create Vulkan gui pipeline layout.");
        return false;
    }

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
            vk::VertexInputRate::eVertex),
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

    vk::Viewport viewport(0, 0, DEFAULT_RES_WIDTH, DEFAULT_RES_HEIGHT, 0.0f, 1.0f);
    vk::Rect2D scissor(vk::Offset2D(0, 0), vk::Extent2D(DEFAULT_RES_WIDTH, DEFAULT_RES_HEIGHT));

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
            vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA),
        vk::PipelineColorBlendAttachmentState(
            true, // Enable Blending
            vk::BlendFactor::eSrcColor, // Src Color
            vk::BlendFactor::eDstColor, // Dst Color
            vk::BlendOp::eAdd, // Color Blend Op
            vk::BlendFactor::eOneMinusSrcAlpha, // Src Alpha
            vk::BlendFactor::eDstAlpha, // Dst Alpha
            vk::BlendOp::eAdd, // Alpha Blend Op
            vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA),
    };

    vk::PipelineColorBlendStateCreateInfo gui_pipeline_blend_info(
        vk::PipelineColorBlendStateCreateFlags(), // No Flags
        false, vk::LogicOp::eClear, // No Logic Op
        attachment_blending.size(), attachment_blending.data(), // Blending Attachments
        { 1, 1, 1, 1 } // Blend Constants
    );

    std::vector<vk::DynamicState> dynamic_states = {
//        vk::DynamicState::eViewport,
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
        0);

    state.gui_vulkan.pipeline = state.device.createGraphicsPipeline(vk::PipelineCache(), gui_pipeline_info, nullptr);
    if (!state.gui_vulkan.pipeline) {
        LOG_ERROR("Failed to create Vulkan gui pipeline.");
        return false;
    }

    if (!ImGui_ImplSdlVulkan_CreateFontsTexture(state)) {
        LOG_ERROR("Failed to create Vulkan gui font texture.");
        return false;
    }

    constexpr size_t mat4_size = sizeof(float) * 4 * 4;

    vk::BufferCreateInfo transformation_buffer_create_info(
        vk::BufferCreateFlags(), // No Flags
        mat4_size, // Size
        vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eUniformBuffer, // Usage Flags
        vk::SharingMode::eExclusive, 0, nullptr // Exclusive Sharing
    );

    state.gui_vulkan.transformation_buffer = renderer::vulkan::create_buffer(state, transformation_buffer_create_info,
        renderer::vulkan::MemoryType::Device, state.gui_vulkan.transformation_allocation);

    // Create Descriptor Pool and Set
    {
        std::vector<vk::DescriptorPoolSize> pool_sizes = {
            vk::DescriptorPoolSize(vk::DescriptorType::eUniformBuffer, 1),
            vk::DescriptorPoolSize(vk::DescriptorType::eCombinedImageSampler, 1),
        };

        vk::DescriptorPoolCreateInfo descriptor_pool_info(
            vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet, // Flags
            1, // Max Sets
            pool_sizes.size(), pool_sizes.data() // Pool Sizes
        );

        state.gui_vulkan.descriptor_pool = state.device.createDescriptorPool(descriptor_pool_info);

        vk::DescriptorSetAllocateInfo descriptor_info(
            state.gui_vulkan.descriptor_pool, // Descriptor Pool
            1, &state.gui_vulkan.descriptor_set_layout // Set Information
        );

        state.gui_vulkan.descriptor_set = state.device.allocateDescriptorSets(descriptor_info)[0];
        if (!state.gui_vulkan.descriptor_set) {
            LOG_ERROR("Failed to create Vulkan gui descriptor set.");
            return false;
        }

        vk::DescriptorBufferInfo uniform_buffer_info(
            state.gui_vulkan.transformation_buffer, // Buffer
            0, mat4_size // Range
        );

        vk::DescriptorImageInfo sampler_image_info(
            state.gui_vulkan.sampler, // Sampler
            state.gui_vulkan.font_texture_view, // Image View
            vk::ImageLayout::eShaderReadOnlyOptimal // Image Layout
        );

        std::vector<vk::WriteDescriptorSet> write_descriptor_sets = {
            vk::WriteDescriptorSet(
                state.gui_vulkan.descriptor_set, // Descriptor Set
                0, // Binding
                0, 1, // Array Range
                vk::DescriptorType::eUniformBuffer, // Descriptor Type
                nullptr, // Image Info
                &uniform_buffer_info, // Buffer Info
                nullptr // Buffer Texel Views
                ),
            vk::WriteDescriptorSet(
                state.gui_vulkan.descriptor_set, // Descriptor Set
                1, // Binding
                0, 1, // Array Range
                vk::DescriptorType::eCombinedImageSampler, // Descriptor Type
                &sampler_image_info, // Image Info
                nullptr, // Buffer Info
                nullptr // Buffer Texel Views
                )
        };

        state.device.updateDescriptorSets(write_descriptor_sets, {});
    }

    state.gui_vulkan.command_buffer = renderer::vulkan::create_command_buffer(state, renderer::vulkan::CommandType::General);

    vk::FenceCreateInfo next_image_fence_info((vk::FenceCreateFlags()));
    state.gui_vulkan.next_image_fence = state.device.createFence(next_image_fence_info);

    return true;
}
