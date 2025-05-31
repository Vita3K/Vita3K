// Vita3K emulator project
// Copyright (C) 2025 Vita3K team
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
    device.destroy(fragment_shader);
    device.destroy(vertex_shader);

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
    vao.size = sizeof(screen_vertices_t) * screen.swapchain_size;
    vao.init_buffer(vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst);

    // create and zero-fill uvs
    last_uvs.resize(screen.swapchain_size);
    std::fill(last_uvs.begin(), last_uvs.end(), std::array<float, 4>());
}

void SinglePassScreenFilter::create_graphics_pipeline() {
    // create shader modules

    fs::path builtin_shaders_path = screen.state.static_assets / "shaders-builtin/vulkan";
    const auto vertex_shader_path = builtin_shaders_path / get_vertex_name();
    const auto fragment_shader_path = builtin_shaders_path / get_fragment_name();

    vertex_shader = vkutil::load_shader(screen.state.device, vertex_shader_path);
    fragment_shader = vkutil::load_shader(screen.state.device, fragment_shader_path);
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
        .renderPass = screen.default_render_pass,
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
        vk::Rect2D vk_scissor{
            .offset = { 0, 0 },
            .extent = screen.extent
        };
        screen.current_cmd_buffer.setScissor(0, vk_scissor);
        vk::Viewport vk_viewport{
            .minDepth = 0.0f,
            .maxDepth = 1.0f
        };
        // compute viewport now
        const float window_aspect = static_cast<float>(screen.extent.width) / screen.extent.height;
        constexpr float vita_aspect = static_cast<float>(DEFAULT_RES_WIDTH) / DEFAULT_RES_HEIGHT;
        const bool fullscreen_hd_res_pixel_perfect_en = screen.state.fullscreen_hd_res_pixel_perfect && screen.state.fullscreen && !(screen.extent.width % DEFAULT_RES_WIDTH) && !(screen.extent.height % (DEFAULT_RES_HEIGHT - 4));
        if (screen.state.stretch_the_display_area && !fullscreen_hd_res_pixel_perfect_en) {
            // Match the aspect ratio to the screen size.
            vk_viewport.width = static_cast<float>(screen.extent.width);
            vk_viewport.height = static_cast<float>(screen.extent.height);
            vk_viewport.x = 0.0f;
            vk_viewport.y = 0.0f;
        } else if ((window_aspect > vita_aspect) && !fullscreen_hd_res_pixel_perfect_en) {
            // Window is wide. Pin top and bottom.
            vk_viewport.width = screen.extent.height * vita_aspect;
            vk_viewport.height = static_cast<float>(screen.extent.height);
            vk_viewport.x = (screen.extent.width - vk_viewport.width) / 2.0f;
            vk_viewport.y = 0.0f;
        } else {
            // Window is tall. Pin left and right.
            vk_viewport.width = static_cast<float>(screen.extent.width);
            vk_viewport.height = screen.extent.width / vita_aspect;
            vk_viewport.x = 0.0f;
            vk_viewport.y = (screen.extent.height - vk_viewport.height) / 2;
        }
        screen.current_cmd_buffer.setViewport(0, vk_viewport);
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

std::string_view BicubicScreenFilter::get_fragment_name() {
    return "render_main_bicubic.frag.spv";
}

vk::Sampler BicubicScreenFilter::create_sampler() {
    vk::SamplerCreateInfo sampler_info{
        .magFilter = vk::Filter::eNearest,
        .minFilter = vk::Filter::eNearest,
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
        .magFilter = vk::Filter::eLinear,
        .minFilter = vk::Filter::eLinear,
        .addressModeU = vk::SamplerAddressMode::eClampToEdge,
        .addressModeV = vk::SamplerAddressMode::eClampToEdge,
        .addressModeW = vk::SamplerAddressMode::eClampToEdge,
    };
    return screen.state.device.createSampler(sampler_info);
}

struct EasuConstant {
    Viewport viewport;
    vk::Extent2D output_size;
};

struct RcasConstant {
    vk::Extent2D offset;
    float sharpening;
};

FSRScreenFilter::~FSRScreenFilter() {
    vk::Device device = screen.state.device;
    device.waitIdle();

    device.destroy(sampler);

    device.destroy(pipeline_easu);
    device.destroy(pipeline_rcas);
    device.destroy(pipeline_layout_easu);
    device.destroy(pipeline_layout_rcas);
    device.freeDescriptorSets(descriptor_pool, descriptor_sets);
    device.destroy(descriptor_pool);
    device.destroy(descriptor_set_layout);

    device.destroy(rcas_shader);
    device.destroy(easu_shader);
}

void FSRScreenFilter::init() {
    vk::Device device = screen.state.device;

    // create sampler
    vk::SamplerCreateInfo sampler_info{
        .magFilter = vk::Filter::eLinear,
        .minFilter = vk::Filter::eLinear,
        .addressModeU = vk::SamplerAddressMode::eClampToEdge,
        .addressModeV = vk::SamplerAddressMode::eClampToEdge,
        .addressModeW = vk::SamplerAddressMode::eClampToEdge,
    };
    sampler = device.createSampler(sampler_info);

    fs::path builtin_shaders_path = screen.state.static_assets / "shaders-builtin/vulkan";

    const auto easu_shader_path = builtin_shaders_path / "fsr_filter_easu.comp.spv";
    const auto frcas_shader_path = builtin_shaders_path / "fsr_filter_rcas.comp.spv";

    easu_shader = vkutil::load_shader(screen.state.device, easu_shader_path);
    rcas_shader = vkutil::load_shader(screen.state.device, frcas_shader_path);

    std::array<vk::DescriptorSetLayoutBinding, 3> layout_bindings = {
        // src img
        vk::DescriptorSetLayoutBinding{
            .binding = 1,
            .descriptorType = vk::DescriptorType::eSampledImage,
            .descriptorCount = 1,
            .stageFlags = vk::ShaderStageFlagBits::eCompute },
        // dst img
        vk::DescriptorSetLayoutBinding{
            .binding = 2,
            .descriptorType = vk::DescriptorType::eStorageImage,
            .descriptorCount = 1,
            .stageFlags = vk::ShaderStageFlagBits::eCompute },
        // src img sampler
        vk::DescriptorSetLayoutBinding{
            .binding = 3,
            .descriptorType = vk::DescriptorType::eSampler,
            .descriptorCount = 1,
            .stageFlags = vk::ShaderStageFlagBits::eCompute,
            .pImmutableSamplers = &sampler },
    };

    vk::DescriptorSetLayoutCreateInfo layout_create_info{};
    layout_create_info.setBindings(layout_bindings);
    descriptor_set_layout = device.createDescriptorSetLayout(layout_create_info);

    std::array<vk::DescriptorPoolSize, 3> pool_sizes{
        vk::DescriptorPoolSize{
            .type = vk::DescriptorType::eSampledImage,
            .descriptorCount = screen.swapchain_size * 2 },
        vk::DescriptorPoolSize{
            .type = vk::DescriptorType::eStorageImage,
            .descriptorCount = screen.swapchain_size * 2 },
        vk::DescriptorPoolSize{
            .type = vk::DescriptorType::eSampler,
            .descriptorCount = screen.swapchain_size * 2 },
    };
    vk::DescriptorPoolCreateInfo pool_info{
        .maxSets = screen.swapchain_size * 2,
    };
    pool_info.setPoolSizes(pool_sizes);
    descriptor_pool = device.createDescriptorPool(pool_info);

    vk::DescriptorSetAllocateInfo descr_set_info{
        .descriptorPool = descriptor_pool,
    };
    std::vector<vk::DescriptorSetLayout> descr_set_layouts(screen.swapchain_size * 2, descriptor_set_layout);
    descr_set_info.setSetLayouts(descr_set_layouts);
    descriptor_sets = device.allocateDescriptorSets(descr_set_info);

    vk::PipelineLayoutCreateInfo layout_info{};
    layout_info.setSetLayouts(descriptor_set_layout);
    vk::PushConstantRange push_constant{
        .stageFlags = vk::ShaderStageFlagBits::eCompute,
        .offset = 0,
        .size = sizeof(EasuConstant),
    };
    layout_info.setPushConstantRanges(push_constant);
    pipeline_layout_easu = device.createPipelineLayout(layout_info);

    push_constant.size = sizeof(RcasConstant);
    pipeline_layout_rcas = device.createPipelineLayout(layout_info);

    // create easu and rcas pipelines
    vk::ComputePipelineCreateInfo compute_info{
        .stage = {
            .stage = vk::ShaderStageFlagBits::eCompute,
            .module = easu_shader,
            .pName = "main" },
        .layout = pipeline_layout_easu
    };
    auto result = device.createComputePipeline(nullptr, compute_info);
    if (result.result != vk::Result::eSuccess)
        LOG_ERROR("Failed to create compute pipeline");
    pipeline_easu = result.value;

    compute_info.stage.module = rcas_shader;
    compute_info.layout = pipeline_layout_rcas;
    result = device.createComputePipeline(nullptr, compute_info);
    if (result.result != vk::Result::eSuccess)
        LOG_ERROR("Failed to create compute pipeline");
    pipeline_rcas = result.value;

    // create intermediate images
    intermediate_images.resize(screen.swapchain_size);
    for (auto &img : intermediate_images)
        img.format = vk::Format::eR8G8B8A8Unorm;

    on_resize();
}

void FSRScreenFilter::on_resize() {
    // compute the extent
    const float window_aspect = static_cast<float>(screen.extent.width) / screen.extent.height;
    const float vita_aspect = static_cast<float>(DEFAULT_RES_WIDTH) / DEFAULT_RES_HEIGHT;
    const bool fullscreen_hd_res_pixel_perfect_en = screen.state.fullscreen_hd_res_pixel_perfect & screen.state.fullscreen & !(screen.extent.width % DEFAULT_RES_WIDTH) & !(screen.extent.height % (DEFAULT_RES_HEIGHT - 4));
    if (screen.state.stretch_the_display_area && !fullscreen_hd_res_pixel_perfect_en) {
        // Match the aspect ratio to the screen size.
        output_size.width = static_cast<float>(screen.extent.width);
        output_size.height = static_cast<float>(screen.extent.height);
        output_offset.width = 0.0f;
        output_offset.height = 0.0f;
    } else if ((window_aspect > vita_aspect) && !fullscreen_hd_res_pixel_perfect_en) {
        // Window is wide. Pin top and bottom.
        output_size.width = static_cast<uint32_t>(std::round(screen.extent.height * vita_aspect));
        output_size.height = screen.extent.height;
        output_offset.width = static_cast<uint32_t>(std::round((screen.extent.width - output_size.width) / 2.0f));
        output_offset.height = 0.0;
    } else {
        // Window is tall. Pin left and right.
        output_size.width = screen.extent.width;
        output_size.height = static_cast<uint32_t>(std::round(screen.extent.width / vita_aspect));
        output_offset.width = 0.0f;
        output_offset.height = static_cast<uint32_t>(std::round((screen.extent.height - output_size.height) / 2.0f));
    }

    // recreate the intermediate images
    for (auto &img : intermediate_images) {
        img.destroy();
        img.width = output_size.width;
        img.height = output_size.height;
        img.init_image(vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eSampled);
    }

    // update the descriptor sets (except the first sampler image as it is not fixed)
    std::vector<vk::DescriptorImageInfo> descr_images(screen.swapchain_size * 3);
    std::vector<vk::WriteDescriptorSet> write_descr(screen.swapchain_size * 3);
    for (size_t i = 0; i < write_descr.size(); i++) {
        descr_images[i].imageView = intermediate_images[i / 3].view;
        write_descr[i].setImageInfo(descr_images[i]);
    }

    for (uint32_t i = 0; i < screen.swapchain_size; i++) {
        // easu dst
        descr_images[i * 3].imageLayout = vk::ImageLayout::eGeneral;
        write_descr[i * 3]
            .setDstSet(descriptor_sets[i * 2])
            .setDstBinding(2)
            .setDescriptorType(vk::DescriptorType::eStorageImage);
        // rcas src
        descr_images[i * 3 + 1].imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
        write_descr[i * 3 + 1]
            .setDstSet(descriptor_sets[i * 2 + 1])
            .setDstBinding(1)
            .setDescriptorType(vk::DescriptorType::eSampledImage);
        // rcas dst
        descr_images[i * 3 + 2]
            .setImageView(screen.swapchain_views[i])
            .setImageLayout(vk::ImageLayout::eGeneral);
        write_descr[i * 3 + 2]
            .setDstSet(descriptor_sets[i * 2 + 1])
            .setDstBinding(2)
            .setDescriptorType(vk::DescriptorType::eStorageImage);
    }
    screen.state.device.updateDescriptorSets(write_descr, {});
}

void FSRScreenFilter::render(bool is_pre_renderpass, vk::ImageView src_img, vk::ImageLayout src_layout, const Viewport &viewport) {
    if (!is_pre_renderpass)
        // we are using compute shaders
        return;

    // update descriptor set, (only the easu src)
    vk::DescriptorImageInfo descr_image_info{
        .imageView = src_img,
        .imageLayout = src_layout,
    };
    vk::WriteDescriptorSet write_descr{
        .dstSet = descriptor_sets[2 * screen.swapchain_image_idx],
        .dstBinding = 1,
        .dstArrayElement = 0,
        .descriptorType = vk::DescriptorType::eSampledImage,
    };
    write_descr.setImageInfo(descr_image_info);
    screen.state.device.updateDescriptorSets(write_descr, {});

    vk::CommandBuffer cmd_buffer = screen.current_cmd_buffer;
    // first, make a barrier to make sure we can write to the intermediate texture
    // we don't care about the previous content
    intermediate_images[screen.swapchain_image_idx].transition_to_discard(cmd_buffer, vkutil::ImageLayout::StorageImage);

    // upscaling pass
    cmd_buffer.bindPipeline(vk::PipelineBindPoint::eCompute, pipeline_easu);
    cmd_buffer.bindDescriptorSets(vk::PipelineBindPoint::eCompute, pipeline_layout_easu, 0, descriptor_sets[2 * screen.swapchain_image_idx], {});
    // push the viewport to the shader
    EasuConstant easu_constant{
        .viewport = viewport,
        .output_size = output_size
    };
    cmd_buffer.pushConstants(pipeline_layout_easu, vk::ShaderStageFlagBits::eCompute, 0, sizeof(EasuConstant), &easu_constant);
    const int dispatch_x = (output_size.width + 15) / 16;
    const int dispatch_y = (output_size.height + 15) / 16;
    cmd_buffer.dispatch(dispatch_x, dispatch_y, 1);

    // meanwhile, we need to clear the swapchain surface
    vk::ImageMemoryBarrier barrier{
        .srcAccessMask = vk::AccessFlagBits::eColorAttachmentWrite,
        .dstAccessMask = vk::AccessFlagBits::eTransferWrite,
        .oldLayout = vk::ImageLayout::eUndefined,
        .newLayout = vk::ImageLayout::eTransferDstOptimal,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = screen.swapchain_images[screen.swapchain_image_idx],
        .subresourceRange = vkutil::color_subresource_range
    };
    cmd_buffer.pipelineBarrier(vk::PipelineStageFlagBits::eColorAttachmentOutput, vk::PipelineStageFlagBits::eTransfer,
        vk::DependencyFlags(), {}, {}, barrier);

    vk::ClearColorValue clear_color{ std::array<float, 4>({ 0.0f, 0.0f, 0.0f, 0.0f }) };
    cmd_buffer.clearColorImage(screen.swapchain_images[screen.swapchain_image_idx], vk::ImageLayout::eTransferDstOptimal, clear_color, vkutil::color_subresource_range);

    // then transition the read texture to sampled // wait for the previous compute shader to be done
    intermediate_images[screen.swapchain_image_idx].transition_to(cmd_buffer, vkutil::ImageLayout::SampledImage);

    // also transition the swapchain image to general
    barrier = {
        .srcAccessMask = vk::AccessFlagBits::eTransferWrite,
        .dstAccessMask = vk::AccessFlagBits::eShaderWrite,
        .oldLayout = vk::ImageLayout::eTransferDstOptimal,
        .newLayout = vk::ImageLayout::eGeneral,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = screen.swapchain_images[screen.swapchain_image_idx],
        .subresourceRange = vkutil::color_subresource_range
    };
    cmd_buffer.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eComputeShader,
        vk::DependencyFlags(), {}, {}, barrier);

    // sharpening pass
    cmd_buffer.bindPipeline(vk::PipelineBindPoint::eCompute, pipeline_rcas);
    cmd_buffer.bindDescriptorSets(vk::PipelineBindPoint::eCompute, pipeline_layout_rcas, 0, descriptor_sets[2 * screen.swapchain_image_idx + 1], {});
    RcasConstant rcas_constant{
        .offset = output_offset,
        // some default value for sharpening
        .sharpening = 0.2f
    };
    cmd_buffer.pushConstants(pipeline_layout_rcas, vk::ShaderStageFlagBits::eCompute, 0, sizeof(RcasConstant), &rcas_constant);
    cmd_buffer.dispatch(dispatch_x, dispatch_y, 1);

    // the barrier for the render pass will be handled by the renderpass external dependencies
}

} // namespace renderer::vulkan
