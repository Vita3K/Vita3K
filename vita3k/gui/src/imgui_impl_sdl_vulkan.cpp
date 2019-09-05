#include <gui/imgui_impl_sdl_vulkan.h>

#include <renderer/types.h>
#include <renderer/vulkan/functions.h>
#include <renderer/vulkan/state.h>

#include <util/log.h>

#include <SDL_vulkan.h>

#include <fstream>

// 200000000 Nanoseconds = 0.2 seconds
constexpr uint64_t next_image_timeout = 200000000;

const std::array<float, 4> clear_color = { 0.0f, 0.0f, 0.0f, 1.0f };
const vk::ClearValue clear_value((vk::ClearColorValue(clear_color)));

// This is seperated because I use similar objects a lot and it is getting irritating to type.
const vk::ImageSubresourceRange base_subresource_range = vk::ImageSubresourceRange(
    vk::ImageAspectFlagBits::eColor, // Aspect
    0, 1, // Level Range
    0, 1 // Layer Range
);

constexpr vk::IndexType imgui_index_type = sizeof(ImDrawIdx) == 2 ? vk::IndexType::eUint16 : vk::IndexType::eUint32;

struct TextureState {
    VmaAllocation allocation = VK_NULL_HANDLE;
    vk::Image image;
    vk::ImageView image_view;
    vk::DescriptorSet descriptor_set;
};

inline renderer::vulkan::VulkanState &get_renderer(ImGui_VulkanState &state) {
    return dynamic_cast<renderer::vulkan::VulkanState &>(*state.renderer);
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

IMGUI_API ImGui_VulkanState *ImGui_ImplSdlVulkan_Init(renderer::State *renderer, SDL_Window *window, const std::string &base_path) {
    auto *state = new ImGui_VulkanState;
    state->renderer = renderer;
    state->window = window;

    state->vertex_module = load_shader(get_renderer(*state).device, base_path + "shaders-builtin/vulkan_imgui_vert.spv");
    state->fragment_module = load_shader(get_renderer(*state).device, base_path + "shaders-builtin/vulkan_imgui_frag.spv");

    return state;
}

IMGUI_API void ImGui_ImplSdlVulkan_Shutdown(ImGui_VulkanState &state) {
    ImGui_ImplSdlVulkan_InvalidateDeviceObjects(state);

    get_renderer(state).device.destroy(state.vertex_module);
    get_renderer(state).device.destroy(state.fragment_module);
}

static void ImGui_ImplSdlVulkan_DeletePipeline(ImGui_VulkanState &state) {
    get_renderer(state).device.destroy(state.pipeline);
    for (vk::Framebuffer framebuffer : state.framebuffers)
        get_renderer(state).device.destroy(framebuffer);
    get_renderer(state).device.destroy(state.renderpass);
}

static bool ImGui_ImplSdlVulkan_CreatePipeline(ImGui_VulkanState &state) {
    // Create GUI Renderpass
    vk::AttachmentDescription attachment_description(
        vk::AttachmentDescriptionFlags(), // No Flags
        vk::Format::eB8G8R8A8Unorm, // Format, BGRA is standard
        vk::SampleCountFlagBits::e1, // No Multisampling
        vk::AttachmentLoadOp::eClear, // Clear Image
        vk::AttachmentStoreOp::eStore, // Keep Image Data
        vk::AttachmentLoadOp::eDontCare, // No Stencils
        vk::AttachmentStoreOp::eDontCare, // No Stencils
        vk::ImageLayout::eUndefined, // Initial Layout
        vk::ImageLayout::eColorAttachmentOptimal // Final Layout
    );

    vk::AttachmentReference attachment_reference(
        0, // attachments[0]
        vk::ImageLayout::eColorAttachmentOptimal // Image Layout
    );

    std::vector<vk::SubpassDescription> subpasses = {
        vk::SubpassDescription(
            vk::SubpassDescriptionFlags(), // No Flags
            vk::PipelineBindPoint::eGraphics, // Type
            0, nullptr, // No Inputs
            1, &attachment_reference, // Color Attachment References
            nullptr, nullptr, // No Resolve or Depth/Stencil for now
            0, nullptr // Vulkan Book says you don't need this for a Color Attachment?
            )
    };

    vk::RenderPassCreateInfo renderpass_info(
        vk::RenderPassCreateFlags(), // No Flags
        1, &attachment_description, // Attachments
        subpasses.size(), subpasses.data(), // Subpasses
        0, nullptr // Dependencies
    );

    state.renderpass = get_renderer(state).device.createRenderPass(renderpass_info, nullptr);
    if (!state.renderpass) {
        LOG_ERROR("Failed to create Vulkan gui renderpass.");
        return false;
    }

    // Create Framebuffer
    for (uint32_t a = 0; a < 2; a++) {
        vk::FramebufferCreateInfo framebuffer_info(
            vk::FramebufferCreateFlags(), // No Flags
            state.renderpass, // Renderpass
            1, &get_renderer(state).swapchain_views[a], // Attachments
            get_renderer(state).swapchain_width, get_renderer(state).swapchain_height, // Size
            1 // Layers
        );

        state.framebuffers[a] = get_renderer(state).device.createFramebuffer(framebuffer_info, nullptr);
    }

    std::vector<vk::PipelineShaderStageCreateInfo> shader_stage_infos = {
        vk::PipelineShaderStageCreateInfo(
            vk::PipelineShaderStageCreateFlags(), // No Flags
            vk::ShaderStageFlagBits::eVertex, // Vertex Shader
            state.vertex_module, // Module
            "main", // Name
            nullptr // Specialization
            ),
        vk::PipelineShaderStageCreateInfo(
            vk::PipelineShaderStageCreateFlags(), // No Flags
            vk::ShaderStageFlagBits::eFragment, // Fragment Shader
            state.fragment_module, // Module
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

    vk::Viewport viewport(0, 0, get_renderer(state).swapchain_width, get_renderer(state).swapchain_height, 0.0f, 1.0f);
    vk::Rect2D scissor(vk::Offset2D(0, 0), vk::Extent2D(get_renderer(state).swapchain_width, get_renderer(state).swapchain_height));

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

    vk::PipelineColorBlendAttachmentState attachment_blending(
        true, // Enable Blending
        vk::BlendFactor::eSrcAlpha, // Src Color
        vk::BlendFactor::eOneMinusSrcAlpha, // Dst Color
        vk::BlendOp::eAdd, // Color Blend Op
        vk::BlendFactor::eOne, // Src Alpha
        vk::BlendFactor::eZero, // Dst Alpha
        vk::BlendOp::eAdd, // Alpha Blend Op
        vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA);

    vk::PipelineColorBlendStateCreateInfo gui_pipeline_blend_info(
        vk::PipelineColorBlendStateCreateFlags(), // No Flags
        false, vk::LogicOp::eClear, // No Logic Op
        1, &attachment_blending, // Blending Attachments
        { 1, 1, 1, 1 } // Blend Constants
    );

    std::vector<vk::DynamicState> dynamic_states = {
        vk::DynamicState::eScissor,
        vk::DynamicState::eViewport,
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
        state.pipeline_layout,
        state.renderpass,
        0,
        vk::Pipeline(),
        0);

    state.pipeline = get_renderer(state).device.createGraphicsPipeline(vk::PipelineCache(), gui_pipeline_info, nullptr);
    if (!state.pipeline) {
        LOG_ERROR("Failed to create Vulkan gui pipeline.");
        return false;
    }

    return true;
}

// Only one mapping can be created on a section of memory at a time. This method is split into the "vertex" and "index" parts to avoid overlapping maps.
static void ImGui_ImplSdlVulkan_UpdateBuffers(ImGui_VulkanState &state, ImDrawData *draw_data) {
    VkResult result;

    ImDrawVert *draw_buffer_data = nullptr;

    if (state.draw_buffer_vertices != draw_data->TotalVtxCount) {
        // Recreate Buffer
        if (state.draw_buffer)
            renderer::vulkan::destroy_buffer(get_renderer(state), state.draw_buffer, state.draw_allocation);

        vk::BufferCreateInfo draw_buffer_info(
            vk::BufferCreateFlags(), // No Flags
            draw_data->TotalVtxCount * sizeof(ImDrawVert), // Size
            vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eVertexBuffer, // Usage
            vk::SharingMode::eExclusive, 0, nullptr // Exclusive Sharing Mode
        );

        state.draw_buffer = renderer::vulkan::create_buffer(get_renderer(state), draw_buffer_info,
            renderer::vulkan::MemoryType::Mappable, state.draw_allocation);

        state.draw_buffer_vertices = draw_data->TotalVtxCount;
    }

    result = vmaMapMemory(get_renderer(state).allocator, state.draw_allocation,
        reinterpret_cast<void **>(&draw_buffer_data));
    vmaInvalidateAllocation(get_renderer(state).allocator, state.draw_allocation,
        0, draw_data->TotalVtxCount * sizeof(ImDrawVert));
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

    vmaFlushAllocation(get_renderer(state).allocator, state.draw_allocation,
        0, draw_data->TotalVtxCount * sizeof(ImDrawVert));
    vmaUnmapMemory(get_renderer(state).allocator, state.draw_allocation);

    // Write to index buffer...
    ImDrawIdx *index_buffer_data = nullptr;

    if (state.index_buffer_indices != draw_data->TotalIdxCount) {
        // Recreate Buffer
        if (state.index_buffer)
            renderer::vulkan::destroy_buffer(get_renderer(state), state.index_buffer, state.index_allocation);

        vk::BufferCreateInfo index_buffer_info(
            vk::BufferCreateFlags(), // No Flags
            draw_data->TotalIdxCount * sizeof(ImDrawIdx), // Size
            vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eIndexBuffer, // Usage
            vk::SharingMode::eExclusive, 0, nullptr // Exclusive Sharing Mode
        );

        state.index_buffer = renderer::vulkan::create_buffer(get_renderer(state), index_buffer_info,
            renderer::vulkan::MemoryType::Mappable, state.index_allocation);

        state.index_buffer_indices = draw_data->TotalIdxCount;
    }

    result = vmaMapMemory(get_renderer(state).allocator, state.index_allocation,
        reinterpret_cast<void **>(&index_buffer_data));
    vmaInvalidateAllocation(get_renderer(state).allocator, state.index_allocation,
        0, draw_data->TotalIdxCount * sizeof(ImDrawIdx));
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

    vmaFlushAllocation(get_renderer(state).allocator, state.index_allocation,
        0, draw_data->TotalIdxCount * sizeof(ImDrawIdx));
    vmaUnmapMemory(get_renderer(state).allocator, state.index_allocation);
}

IMGUI_API void ImGui_ImplSdlVulkan_RenderDrawData(ImGui_VulkanState &state) {
    ImDrawData *draw_data = ImGui::GetDrawData();

    uint32_t image_index = ~0u;
    vk::Result acquire_result = get_renderer(state).device.acquireNextImageKHR(get_renderer(state).swapchain,
        next_image_timeout, state.image_acquired_semaphore, vk::Fence(), &image_index);

    while (acquire_result == vk::Result::eErrorOutOfDateKHR) {
        // This whole acquire thing should probably be moved out of imgui since renderering will also happen elsewhere.
        int width, height;
        SDL_Vulkan_GetDrawableSize(state.window, &width, &height);
        renderer::vulkan::resize_swapchain(get_renderer(state), vk::Extent2D(width, height));
        ImGui_ImplSdlVulkan_DeletePipeline(state);
        ImGui_ImplSdlVulkan_CreatePipeline(state);

        acquire_result = get_renderer(state).device.acquireNextImageKHR(get_renderer(state).swapchain,
            next_image_timeout, state.image_acquired_semaphore, vk::Fence(), &image_index);
    }

    if (acquire_result != vk::Result::eSuccess) {
        LOG_WARN("Failed to get next image. Error: {}", static_cast<VkResult>(acquire_result));
        return;
    }

    ImGui_ImplSdlVulkan_UpdateBuffers(state, draw_data);

    state.command_buffer.reset(vk::CommandBufferResetFlags());

    vk::CommandBufferBeginInfo begin_info(
        vk::CommandBufferUsageFlags(), // No Flags
        nullptr // Inheritance
    );

    state.command_buffer.begin(begin_info);

    const float matrix[] = {
        2.0f / draw_data->DisplaySize.x, 0, 0, 0,
        0, 2.0f / draw_data->DisplaySize.y, 0, 0,
        0, 0, 1, 0,
        -1, -1, 0, 1,
    };

    state.command_buffer.updateBuffer(state.transformation_buffer, 0, sizeof(matrix), matrix);

    vk::RenderPassBeginInfo renderpass_begin_info(
        state.renderpass, // Renderpass
        state.framebuffers[image_index], // Framebuffer
        vk::Rect2D(
            vk::Offset2D(0, 0),
            vk::Extent2D(get_renderer(state).swapchain_width, get_renderer(state).swapchain_height)),
        1, &clear_value // Clear Colors
    );

    state.command_buffer.beginRenderPass(renderpass_begin_info, vk::SubpassContents::eInline);
    state.command_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, state.pipeline);
    state.command_buffer.bindDescriptorSets(
        vk::PipelineBindPoint::eGraphics, // Bind Point
        state.pipeline_layout, // Layout
        0, // First Set
        1, &state.matrix_set, // Sets
        0, nullptr // Dynamic Offsets
    );

    vk::Viewport viewport(draw_data->DisplayPos.x, draw_data->DisplayPos.y,
        draw_data->DisplaySize.x, draw_data->DisplaySize.y, 0.0f, 1.0f);
    state.command_buffer.setViewport(0, 1, &viewport);

    uint64_t vertex_offset_null = 0;

    state.command_buffer.bindVertexBuffers(0, 1, &state.draw_buffer, &vertex_offset_null);
    state.command_buffer.bindIndexBuffer(state.index_buffer, 0, imgui_index_type);

    uint64_t vertex_offset = 0;
    uint64_t index_offset = 0;

    for (uint32_t a = 0; a < draw_data->CmdListsCount; a++) {
        ImDrawList *draw_list = draw_data->CmdLists[a];

        for (const auto &cmd : draw_list->CmdBuffer) {
            if (cmd.UserCallback) {
                cmd.UserCallback(draw_list, &cmd);
            } else {
                vk::Rect2D scissor_rect(
                    vk::Offset2D(cmd.ClipRect.x, cmd.ClipRect.y),
                    vk::Extent2D(cmd.ClipRect.z, cmd.ClipRect.w));
                state.command_buffer.setScissor(0, 1, &scissor_rect);
                auto *texture = reinterpret_cast<TextureState *>(cmd.TextureId);
                state.command_buffer.bindDescriptorSets(
                    vk::PipelineBindPoint::eGraphics, // Bind Point
                    state.pipeline_layout, // Layout
                    1,
                    1, &texture->descriptor_set,
                    0, nullptr);
                state.command_buffer.drawIndexed(cmd.ElemCount, 1, index_offset, vertex_offset, 0);
            }
            index_offset += cmd.ElemCount;
        }

        vertex_offset += draw_list->VtxBuffer.Size;
    }

    state.command_buffer.endRenderPass();

    vk::ImageMemoryBarrier color_attachment_present_barrier(
        vk::AccessFlagBits::eColorAttachmentWrite, // From rendering
        vk::AccessFlagBits::eMemoryRead, // To readable format for presenting
        vk::ImageLayout::eColorAttachmentOptimal,
        vk::ImageLayout::ePresentSrcKHR,
        VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, // No Transfer
        get_renderer(state).swapchain_images[image_index], // Image
        base_subresource_range);

    state.command_buffer.pipelineBarrier(
        vk::PipelineStageFlagBits::eColorAttachmentOutput, vk::PipelineStageFlagBits::eBottomOfPipe, // Color Attachment Output -> Bottom of Pipe Stage
        vk::DependencyFlags(), // No Dependency Flags
        0, nullptr, // No Memory Barriers
        0, nullptr, // No Buffer Barriers
        1, &color_attachment_present_barrier // Image Barrier
    );

    state.command_buffer.end();

    vk::PipelineStageFlags image_wait_stage = vk::PipelineStageFlagBits::eColorAttachmentOutput;

    vk::SubmitInfo submit_info(
        1, &state.image_acquired_semaphore, &image_wait_stage, // Wait Semaphores (wait until image has been acquired to output)
        1, &state.command_buffer, // Command Buffers
        1, &state.render_complete_semaphore // Signal Render Complete Semaphore
    );

    vk::Queue render_queue = renderer::vulkan::select_queue(get_renderer(state), renderer::vulkan::CommandType::General);
    render_queue.submit(1, &submit_info, vk::Fence());

    vk::PresentInfoKHR present_info(
        1, &state.render_complete_semaphore, // Wait Render Complete Semaphore
        1, &get_renderer(state).swapchain, &image_index, nullptr // Swapchain
    );

    vk::Queue present_queue = renderer::vulkan::select_queue(get_renderer(state), renderer::vulkan::CommandType::General);
    present_queue.presentKHR(present_info);
    render_queue.waitIdle();
    present_queue.waitIdle(); // Wait idle is probably bad for performance.
}

IMGUI_API ImTextureID ImGui_ImplSdlVulkan_CreateTexture(ImGui_VulkanState &state, void *pixels, int width, int height) {
    auto *texture = new TextureState;

    const size_t buffer_size = width * height * 4;

    vk::BufferCreateInfo buffer_info(
        vk::BufferCreateFlags(), // No Flags
        buffer_size, // Size
        vk::BufferUsageFlagBits::eTransferSrc, // Usage
        vk::SharingMode::eExclusive, 0, nullptr // Sharing Mode
    );

    VmaAllocation temp_allocation;
    vk::Buffer temp_buffer = renderer::vulkan::create_buffer(get_renderer(state), buffer_info, renderer::vulkan::MemoryType::Mappable, temp_allocation);

    uint8_t *temp_memory;
    VkResult result = vmaMapMemory(get_renderer(state).allocator, temp_allocation, reinterpret_cast<void **>(&temp_memory));
    vmaInvalidateAllocation(get_renderer(state).allocator, temp_allocation, 0, buffer_size);
    if (result != VK_SUCCESS) {
        LOG_ERROR("Could not map font buffer memory. VMA result: {}.", result);
        return nullptr;
    }
    if (!temp_memory) {
        LOG_ERROR("Could not map font buffer memory.");
        return nullptr;
    }
    std::memcpy(temp_memory, pixels, buffer_size);
    vmaFlushAllocation(get_renderer(state).allocator, temp_allocation, 0, buffer_size);
    vmaUnmapMemory(get_renderer(state).allocator, temp_allocation);

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

    texture->image = renderer::vulkan::create_image(get_renderer(state),
        image_info, renderer::vulkan::MemoryType::Device, texture->allocation);

    vk::CommandBuffer transfer_buffer = renderer::vulkan::create_command_buffer(get_renderer(state),
        renderer::vulkan::CommandType::Transfer);

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
        texture->image,
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
        texture->image, // Image
        vk::ImageLayout::eTransferDstOptimal, // Image Layout
        1, &region // Regions
    );

    vk::ImageMemoryBarrier image_shader_read_only_barrier(
        vk::AccessFlagBits::eTransferWrite, // Was just written to.
        vk::AccessFlagBits(), // Will be read by the shader.
        vk::ImageLayout::eTransferDstOptimal, // Old Layout
        vk::ImageLayout::eShaderReadOnlyOptimal, // New Layout
        VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, // Transfer to General Queue (but I don't right now)
        texture->image, // Image
        base_subresource_range // Subresource Range
    );

    transfer_buffer.pipelineBarrier(
        vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eBottomOfPipe, // Transfer -> Bottom of Pipe Stage
        vk::DependencyFlags(), // No Dependency Flags
        0, nullptr, // No Memory Barriers
        0, nullptr, // No Buffer Barriers
        1, &image_shader_read_only_barrier // Image Memory Barriers
    );

    transfer_buffer.end();

    vk::SubmitInfo submit_info(
        0, nullptr, nullptr, // No Wait Semaphores
        1, &transfer_buffer, // Command Buffer
        0, nullptr // No Signal Semaphores
    );
    vk::Queue submit_queue = renderer::vulkan::select_queue(get_renderer(state), renderer::vulkan::CommandType::Transfer);
    submit_queue.submit(1, &submit_info, vk::Fence());
    submit_queue.waitIdle();

    renderer::vulkan::free_command_buffer(get_renderer(state), renderer::vulkan::CommandType::Transfer, transfer_buffer);
    renderer::vulkan::destroy_buffer(get_renderer(state), temp_buffer, temp_allocation);

    vk::ImageViewCreateInfo font_view_info(
        vk::ImageViewCreateFlags(), // No Flags
        texture->image, // Image
        vk::ImageViewType::e2D, // Image View Type
        vk::Format::eR8G8B8A8Unorm, // Image View Format
        vk::ComponentMapping(), // Default Component Mapping (RGBA)
        base_subresource_range // Subresource Range
    );

    texture->image_view = get_renderer(state).device.createImageView(font_view_info);

    vk::DescriptorSetAllocateInfo descriptor_info(
        state.descriptor_pool, // Descriptor Pool
        1, &state.sampler_layout // Layouts
    );

    texture->descriptor_set = get_renderer(state).device.allocateDescriptorSets(descriptor_info)[0];

    vk::DescriptorImageInfo descriptor_image_info(
        state.sampler, // Sampler
        texture->image_view, // Image View
        vk::ImageLayout::eShaderReadOnlyOptimal // Image Layout
    );

    vk::WriteDescriptorSet descriptor_write_info(
        texture->descriptor_set, // Set
        0, // Binding
        0, 1, // Array Range
        vk::DescriptorType::eCombinedImageSampler, // Type
        &descriptor_image_info,
        nullptr,
        nullptr);

    get_renderer(state).device.updateDescriptorSets(1, &descriptor_write_info, 0, nullptr);

    return texture;
}

IMGUI_API void ImGui_ImplSdlVulkan_DeleteTexture(ImGui_VulkanState &state, ImTextureID texture) {
    auto texture_ptr = reinterpret_cast<TextureState *>(texture);

    get_renderer(state).device.free(state.descriptor_pool, 1, &texture_ptr->descriptor_set);
    get_renderer(state).device.destroy(texture_ptr->image_view);
    renderer::vulkan::destroy_image(get_renderer(state), texture_ptr->image, texture_ptr->allocation);

    delete texture_ptr;
}

// Use if you want to reset your rendering device without losing ImGui state.
IMGUI_API void ImGui_ImplSdlVulkan_InvalidateDeviceObjects(ImGui_VulkanState &state) {
    get_renderer(state).device.destroy(state.image_acquired_semaphore);
    get_renderer(state).device.destroy(state.render_complete_semaphore);

    renderer::vulkan::free_command_buffer(get_renderer(state), renderer::vulkan::CommandType::General, state.command_buffer);

    ImGui_ImplSdlVulkan_DeleteTexture(state, state.font_texture);
    ImGui_ImplSdlVulkan_DeletePipeline(state);

    get_renderer(state).device.destroy(state.pipeline_layout);
    get_renderer(state).device.destroy(state.sampler_layout);
    get_renderer(state).device.destroy(state.matrix_layout);

    get_renderer(state).device.destroy(state.sampler);
}

IMGUI_API bool ImGui_ImplSdlVulkan_CreateDeviceObjects(ImGui_VulkanState &state) {
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

    state.sampler = get_renderer(state).device.createSampler(sampler_info, nullptr);
    if (!state.sampler) {
        LOG_ERROR("Failed to create Vulkan gui sampler.");
        return false;
    }

    // Create Layouts
    {
        vk::DescriptorSetLayoutBinding matrix_layout_binding(
            0, // Binding
            vk::DescriptorType::eUniformBuffer, // Descriptor Type
            1, // Array Size
            vk::ShaderStageFlagBits::eVertex, // Usage Stage
            nullptr // Used Samplers
        );

        vk::DescriptorSetLayoutCreateInfo matrix_layout_info(
            vk::DescriptorSetLayoutCreateFlags(), // No Flags
            1, &matrix_layout_binding // Bindings
        );

        state.matrix_layout = get_renderer(state).device.createDescriptorSetLayout(matrix_layout_info, nullptr);
        if (!state.matrix_layout) {
            LOG_ERROR("Failed to create Vulkan gui matrix layout.");
            return false;
        }

        vk::DescriptorSetLayoutBinding sampler_layout(
            0, // Binding
            vk::DescriptorType::eCombinedImageSampler, // Descriptor Type
            1, // Array Size
            vk::ShaderStageFlagBits::eFragment, // Usage Stage
            &state.sampler // Used Samplers
        );

        vk::DescriptorSetLayoutCreateInfo sampler_layout_info(
            vk::DescriptorSetLayoutCreateFlags(), // No Flags
            1, &sampler_layout // Bindings
        );

        state.sampler_layout = get_renderer(state).device.createDescriptorSetLayout(sampler_layout_info, nullptr);
        if (!state.sampler_layout) {
            LOG_ERROR("Failed to create Vulkan gui sampler layout.");
            return false;
        }

        std::vector<vk::DescriptorSetLayout> pipeline_layouts = {
            state.matrix_layout,
            state.sampler_layout,
        };

        vk::PipelineLayoutCreateInfo pipeline_layout_info(
            vk::PipelineLayoutCreateFlags(), // No Flags
            pipeline_layouts.size(), pipeline_layouts.data(), // Descriptor Layouts
            0, nullptr // Push Constants
        );

        state.pipeline_layout = get_renderer(state).device.createPipelineLayout(pipeline_layout_info, nullptr);
        if (!state.pipeline_layout) {
            LOG_ERROR("Failed to create Vulkan gui pipeline layout.");
            return false;
        }
    }

    if (!ImGui_ImplSdlVulkan_CreatePipeline(state))
        return false;

    constexpr size_t mat4_size = sizeof(float) * 4 * 4;

    vk::BufferCreateInfo transformation_buffer_create_info(
        vk::BufferCreateFlags(), // No Flags
        mat4_size, // Size
        vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eUniformBuffer, // Usage Flags
        vk::SharingMode::eExclusive, 0, nullptr // Exclusive Sharing
    );

    state.transformation_buffer = renderer::vulkan::create_buffer(get_renderer(state),
        transformation_buffer_create_info, renderer::vulkan::MemoryType::Device, state.transformation_allocation);

    // Create Descriptor Pool and Set
    {
        std::vector<vk::DescriptorPoolSize> pool_sizes = {
            vk::DescriptorPoolSize(vk::DescriptorType::eUniformBuffer, 1),
            vk::DescriptorPoolSize(vk::DescriptorType::eCombinedImageSampler, 511),
        };

        vk::DescriptorPoolCreateInfo descriptor_pool_info(
            vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet, // Flags
            512, // Max Sets
            pool_sizes.size(), pool_sizes.data() // Pool Sizes
        );

        state.descriptor_pool = get_renderer(state).device.createDescriptorPool(descriptor_pool_info);
        if (!state.descriptor_pool) {
            LOG_ERROR("Failed to create Vulkan gui descriptor pool.");
            return false;
        }

        vk::DescriptorSetAllocateInfo descriptor_info(
            state.descriptor_pool, // Descriptor Pool
            1, &state.matrix_layout // Set Information
        );

        state.matrix_set = get_renderer(state).device.allocateDescriptorSets(descriptor_info)[0];
        if (!state.matrix_set) {
            LOG_ERROR("Failed to create Vulkan gui matrix descriptor set.");
            return false;
        }

        vk::DescriptorBufferInfo uniform_buffer_info(
            state.transformation_buffer, // Buffer
            0, mat4_size // Range
        );

        vk::WriteDescriptorSet matrix_buffer_info(
            state.matrix_set, // Descriptor Set
            0, // Binding
            0, 1, // Array Range
            vk::DescriptorType::eUniformBuffer, // Descriptor Type
            nullptr, // Image Info
            &uniform_buffer_info, // Buffer Info
            nullptr // Buffer Texel Views
        );

        get_renderer(state).device.updateDescriptorSets(1, &matrix_buffer_info, 0, nullptr);
    }

    // Create ImGui Texture
    {
        ImGuiIO &io = ImGui::GetIO();

        uint8_t *pixels;
        int width, height;
        io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);

        state.font_texture = ImGui_ImplSdlVulkan_CreateTexture(state, pixels, width, height);
        io.Fonts->TexID = state.font_texture;
    }

    state.command_buffer = renderer::vulkan::create_command_buffer(get_renderer(state), renderer::vulkan::CommandType::General);

    vk::SemaphoreCreateInfo semaphore_info((vk::SemaphoreCreateFlags()));
    state.image_acquired_semaphore = get_renderer(state).device.createSemaphore(semaphore_info);
    state.render_complete_semaphore = get_renderer(state).device.createSemaphore(semaphore_info);

    state.init = true;

    return true;
}
