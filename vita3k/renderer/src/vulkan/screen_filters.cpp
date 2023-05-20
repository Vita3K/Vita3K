// Vita3K emulator project
// Copyright (C) 2023 Vita3K team
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

#include "renderer/vulkan/screen_filters.h"
#include "renderer/vulkan/screen_renderer.h"

#include "renderer/vulkan/state.h"

namespace renderer::vulkan {

ScreenFilter::ScreenFilter(ScreenRenderer &screen_renderer)
    : screen(screen_renderer) {}

struct screen_vertex {
    float pos[3];
    float uv[2];
};

static constexpr size_t screen_vertex_size = sizeof(screen_vertex);
static constexpr uint32_t screen_vertex_count = 4;

using screen_vertices_t = screen_vertex[screen_vertex_count];

SinglePassScreenFilter::SinglePassScreenFilter(ScreenRenderer &screen)
    : ScreenFilter(screen) {}

SinglePassScreenFilter::~SinglePassScreenFilter() {
    vk::Device device = screen.state.device;
    // this will only happen when the user changes the option in the GUI, we can afford to waitIdle
    device.waitIdle();
    vao.destroy();
    device.destroy(pipeline);
    device.destroy(pipeline_layout);
    device.freeDescriptorSets(descriptor_pool, descriptor_sets);
    device.destroy(descriptor_pool);
    device.destroy(descriptor_set_layout);

    device.destroy(sampler);
}

void SinglePassScreenFilter::create_layout_sync() {
    vk::Device device = screen.state.device;

    vk::DescriptorSetLayoutBinding sampler_layout_binding{
        .binding = 0,
        .descriptorType = vk::DescriptorType::eCombinedImageSampler,
        .descriptorCount = 1,
        .stageFlags = vk::ShaderStageFlagBits::eFragment,
    };
    vk::DescriptorSetLayoutCreateInfo descriptor_info{};
    descriptor_info.setBindings(sampler_layout_binding);
    descriptor_set_layout = device.createDescriptorSetLayout(descriptor_info);

    vk::DescriptorPoolSize pool_size{
        .type = vk::DescriptorType::eCombinedImageSampler,
        .descriptorCount = screen.swapchain_size
    };
    vk::DescriptorPoolCreateInfo pool_info{
        .maxSets = screen.swapchain_size,
    };
    pool_info.setPoolSizes(pool_size);
    descriptor_pool = device.createDescriptorPool(pool_info);

    vk::DescriptorSetAllocateInfo descr_set_info{
        .descriptorPool = descriptor_pool,
    };
    std::vector<vk::DescriptorSetLayout> descr_set_layouts(screen.swapchain_size, descriptor_set_layout);
    descr_set_info.setSetLayouts(descr_set_layouts);
    descriptor_sets = device.allocateDescriptorSets(descr_set_info);

    vk::PipelineLayoutCreateInfo layout_info{};
    layout_info.setSetLayouts(descriptor_set_layout);
    // add push constant for fxaa pipeline, not used by the normal pipeline
    vk::PushConstantRange push_constant{
        .stageFlags = vk::ShaderStageFlagBits::eFragment,
        .offset = 0,
        .size = 2 * sizeof(float),
    };
    layout_info.setPushConstantRanges(push_constant);
    pipeline_layout = device.createPipelineLayout(layout_info);

    // create vao
    vao.allocator = screen.state.allocator;
    vao.size = sizeof(screen_vertices_t) * screen.swapchain_size;
    vao.init_buffer(vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst);

    // create and zero-fill uvs
    last_uvs.resize(screen.swapchain_size);
    std::fill(last_uvs.begin(), last_uvs.end(), std::array<float, 4>());
}

void SinglePassScreenFilter::create_graphics_pipeline() {
    // create shader modules
    const auto builtin_shaders_path = std::string(screen.state.base_path) + "shaders-builtin/vulkan/";
    vertex_shader = vkutil::load_shader(screen.state.device, builtin_shaders_path + std::string(get_vertex_name()));
    fragment_shader = vkutil::load_shader(screen.state.device, builtin_shaders_path + std::string(get_fragment_name()));
    vk::PipelineShaderStageCreateInfo vert_info{
        .stage = vk::ShaderStageFlagBits::eVertex,
        .module = vertex_shader,
        .pName = "main"
    };
    vk::PipelineShaderStageCreateInfo frag_info{
        .stage = vk::ShaderStageFlagBits::eFragment,
        .module = fragment_shader,
        .pName = "main"
    };
    std::array<vk::PipelineShaderStageCreateInfo, 2> shader_stages = { vert_info, frag_info };

    vk::VertexInputBindingDescription binding_descr{
        .binding = 0,
        .stride = screen_vertex_size,
        .inputRate = vk::VertexInputRate::eVertex
    };
    std::array<vk::VertexInputAttributeDescription, 2> attr_descr;
    // pos
    attr_descr[0] = vk::VertexInputAttributeDescription{
        .location = 0,
        .binding = 0,
        .format = vk::Format::eR32G32B32Sfloat,
        .offset = offsetof(screen_vertex, pos)
    };
    // uv
    attr_descr[1] = vk::VertexInputAttributeDescription{
        .location = 1,
        .binding = 0,
        .format = vk::Format::eR32G32Sfloat,
        .offset = offsetof(screen_vertex, uv)
    };
    vk::PipelineVertexInputStateCreateInfo vertex_input{};
    vertex_input.setVertexBindingDescriptions(binding_descr);
    vertex_input.setVertexAttributeDescriptions(attr_descr);

    vk::PipelineInputAssemblyStateCreateInfo input_assembly{
        .topology = vk::PrimitiveTopology::eTriangleStrip
    };
    vk::PipelineViewportStateCreateInfo viewport_state{
        .viewportCount = 1,
        .scissorCount = 1
    };
    vk::PipelineRasterizationStateCreateInfo rasterizer{
        .polygonMode = vk::PolygonMode::eFill,
        .cullMode = vk::CullModeFlagBits::eNone,
        .frontFace = vk::FrontFace::eClockwise,
        .lineWidth = 1.0f
    };
    vk::PipelineMultisampleStateCreateInfo multisampling{
        .rasterizationSamples = vk::SampleCountFlagBits::e1
    };
    vk::PipelineColorBlendAttachmentState blend_attachment{
        .blendEnable = VK_FALSE,
        .colorWriteMask = vkutil::default_color_mask
    };
    vk::PipelineColorBlendStateCreateInfo color_blending{};
    color_blending.setAttachments(blend_attachment);

    // dynamic scissor and viewport are used, in case the extent changes
    static vk::DynamicState dynamic_states[] = {
        vk::DynamicState::eViewport,
        vk::DynamicState::eScissor
    };
    vk::PipelineDynamicStateCreateInfo dynamic_info{};
    dynamic_info.setDynamicStates(dynamic_states);

    vk::GraphicsPipelineCreateInfo pipeline_info{
        .pVertexInputState = &vertex_input,
        .pInputAssemblyState = &input_assembly,
        .pViewportState = &viewport_state,
        .pRasterizationState = &rasterizer,
        .pMultisampleState = &multisampling,
        .pColorBlendState = &color_blending,
        .pDynamicState = &dynamic_info,
        .layout = pipeline_layout,
        .renderPass = screen.render_pass,
        .subpass = 0
    };
    pipeline_info.setStages(shader_stages);

    const auto result = screen.state.device.createGraphicsPipeline(VK_NULL_HANDLE, pipeline_info);
    if (result.result != vk::Result::eSuccess) {
        LOG_CRITICAL("Failed to create pipeline.");
    }
    pipeline = result.value;
}

std::string_view SinglePassScreenFilter::get_vertex_name() {
    return "render_main.vert.spv";
}

std::string_view SinglePassScreenFilter::get_fragment_name() {
    return "render_main.frag.spv";
}

void SinglePassScreenFilter::init() {
    create_layout_sync();
    create_graphics_pipeline();
    this->sampler = create_sampler();
}

void SinglePassScreenFilter::render(bool is_pre_renderpass, vk::ImageView src_img, vk::ImageLayout src_layout, const Viewport &viewport) {
    if (is_pre_renderpass) {
        std::array<float, 4> uvs = {
            viewport.offset_x / (float)viewport.texture_width,
            viewport.offset_y / (float)viewport.texture_height,
            (viewport.offset_x + viewport.width) / (float)(viewport.texture_width),
            (viewport.offset_y + viewport.height) / (float)(viewport.texture_height)
        };

        // if necessary update vao (should not happen often)
        if (uvs != last_uvs[screen.swapchain_image_idx]) {
            screen_vertices_t vertex_buffer_data = {
                { { 1.f, -1.f, 0.0f }, { 1.f, 0.f } },
                { { -1.f, -1.f, 0.0f }, { 0.f, 0.f } },
                { { 1.f, 1.f, 0.0f }, { 1.f, 1.f } },
                { { -1.f, 1.f, 0.0f }, { 0.f, 1.f } },
            };
            vertex_buffer_data[0].uv[0] = uvs[2];
            vertex_buffer_data[0].uv[1] = uvs[1];

            vertex_buffer_data[1].uv[0] = uvs[0];
            vertex_buffer_data[1].uv[1] = uvs[1];

            vertex_buffer_data[2].uv[0] = uvs[2];
            vertex_buffer_data[2].uv[1] = uvs[3];

            vertex_buffer_data[3].uv[0] = uvs[0];
            vertex_buffer_data[3].uv[1] = uvs[3];

            screen.current_cmd_buffer.updateBuffer(vao.buffer, screen.swapchain_image_idx * sizeof(screen_vertices_t), sizeof(screen_vertices_t), &vertex_buffer_data);
            last_uvs[screen.swapchain_image_idx] = uvs;
        }

        return;
    }

    {
        // update descriptor set
        vk::DescriptorImageInfo descr_image_info{
            .sampler = sampler,
            .imageView = src_img,
            .imageLayout = src_layout,
        };
        vk::WriteDescriptorSet write_descr{
            .dstSet = descriptor_sets[screen.swapchain_image_idx],
            .dstBinding = 0,
            .dstArrayElement = 0,
            .descriptorType = vk::DescriptorType::eCombinedImageSampler,
        };
        write_descr.setImageInfo(descr_image_info);
        screen.state.device.updateDescriptorSets(write_descr, {});

        // set viewport and scissor
        vk::Rect2D scissor{
            .offset = { 0, 0 },
            .extent = screen.extent
        };
        screen.current_cmd_buffer.setScissor(0, scissor);
        vk::Viewport viewport{
            .minDepth = 0.0f,
            .maxDepth = 1.0f
        };
        // compute viewport now
        const float window_aspect = static_cast<float>(screen.extent.width) / screen.extent.height;
        const float vita_aspect = static_cast<float>(DEFAULT_RES_WIDTH) / DEFAULT_RES_HEIGHT;
        if (window_aspect > vita_aspect) {
            // Window is wide. Pin top and bottom.
            viewport.width = screen.extent.height * vita_aspect;
            viewport.height = static_cast<float>(screen.extent.height);
            viewport.x = (screen.extent.width - viewport.width) / 2.0f;
            viewport.y = 0.0f;
        } else {
            // Window is tall. Pin left and right.
            viewport.width = static_cast<float>(screen.extent.width);
            viewport.height = screen.extent.width / vita_aspect;
            viewport.x = 0.0f;
            viewport.y = (screen.extent.height - viewport.height) / 2;
        }
        screen.current_cmd_buffer.setViewport(0, viewport);
    }

    {
        vk::DeviceSize offset = screen.swapchain_image_idx * sizeof(screen_vertices_t);
        screen.current_cmd_buffer.bindVertexBuffers(0, vao.buffer, offset);

        screen.current_cmd_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline);
        screen.current_cmd_buffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipeline_layout, 0, descriptor_sets[screen.swapchain_image_idx], {});

        std::array<float, 2> inv_size = { 1.f / viewport.texture_width, 1.f / viewport.texture_height };
        screen.current_cmd_buffer.pushConstants(pipeline_layout, vk::ShaderStageFlagBits::eFragment, 0, 2 * sizeof(float), inv_size.data());

        screen.current_cmd_buffer.draw(4, 1, 0, 0);
    }
}

vk::Sampler NearestScreenFilter::create_sampler() {
    vk::SamplerCreateInfo sampler_info{
        .magFilter = vk::Filter::eNearest,
        .minFilter = vk::Filter::eNearest,
        .addressModeU = vk::SamplerAddressMode::eClampToEdge,
        .addressModeV = vk::SamplerAddressMode::eClampToEdge,
        .addressModeW = vk::SamplerAddressMode::eClampToEdge,
    };
    return screen.state.device.createSampler(sampler_info);
}

vk::Sampler BilinearScreenFilter::create_sampler() {
    vk::SamplerCreateInfo sampler_info{
        .magFilter = vk::Filter::eLinear,
        .minFilter = vk::Filter::eLinear,
        .addressModeU = vk::SamplerAddressMode::eClampToEdge,
        .addressModeV = vk::SamplerAddressMode::eClampToEdge,
        .addressModeW = vk::SamplerAddressMode::eClampToEdge,
    };
    return screen.state.device.createSampler(sampler_info);
}

std::string_view FXAAScreenFilter::get_fragment_name() {
    return "render_main_fxaa.frag.spv";
}

vk::Sampler FXAAScreenFilter::create_sampler() {
    vk::SamplerCreateInfo sampler_info{
        // I'm still unsure whether the FXAA sampler should be nearest or linear...
        .magFilter = vk::Filter::eLinear,
        .minFilter = vk::Filter::eLinear,
        .addressModeU = vk::SamplerAddressMode::eClampToEdge,
        .addressModeV = vk::SamplerAddressMode::eClampToEdge,
        .addressModeW = vk::SamplerAddressMode::eClampToEdge,
    };
    return screen.state.device.createSampler(sampler_info);
}

} // namespace renderer::vulkan
