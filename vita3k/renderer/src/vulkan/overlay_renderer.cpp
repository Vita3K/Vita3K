// Vita3K emulator project
// Copyright (C) 2026 Vita3K team
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
// Copyright RPCS3
// SPDX-License-Identifier: GPL-2.0
// Code heavily referenced/taken from https://github.com/RPCS3/rpcs3/tree/master/rpcs3/Emu/RSX/Overlays

#include <renderer/vulkan/overlay_renderer.h>
#include <renderer/vulkan/state.h>

#include <overlay/font.h>
#include <util/log.h>

#include <algorithm>
#include <chrono>
#include <cmath>

namespace renderer::vulkan {

static constexpr float VIRTUAL_WIDTH = 960.f;
static constexpr float VIRTUAL_HEIGHT = 544.f;

OverlayRenderer::~OverlayRenderer() {
    destroy();
}

bool OverlayRenderer::init(VKState &state) {
    m_state = &state;
    vk::Device device = state.device;

    const fs::path builtin_shaders_path = state.static_assets / "shaders-builtin" / "overlay";
    m_vert_shader = vkutil::load_shader(device, builtin_shaders_path / "overlay.vert.spv");
    m_frag_shader = vkutil::load_shader(device, builtin_shaders_path / "overlay.frag.spv");

    if (!m_vert_shader || !m_frag_shader) {
        LOG_ERROR("Failed to load overlay SPIR-V shaders");
        return false;
    }

    std::array<vk::DescriptorSetLayoutBinding, 2> bindings = {
        vk::DescriptorSetLayoutBinding{
            .binding = 0,
            .descriptorType = vk::DescriptorType::eCombinedImageSampler,
            .descriptorCount = 1,
            .stageFlags = vk::ShaderStageFlagBits::eFragment,
        },
        vk::DescriptorSetLayoutBinding{
            .binding = 1,
            .descriptorType = vk::DescriptorType::eCombinedImageSampler,
            .descriptorCount = 1,
            .stageFlags = vk::ShaderStageFlagBits::eFragment,
        }
    };
    vk::DescriptorSetLayoutCreateInfo desc_layout_info{};
    desc_layout_info.setBindings(bindings);
    m_desc_set_layout = device.createDescriptorSetLayout(desc_layout_info);

    vk::PushConstantRange push_range{
        .stageFlags = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment,
        .offset = 0,
        .size = sizeof(OverlayPushConstants),
    };
    vk::PipelineLayoutCreateInfo layout_info{};
    layout_info.setSetLayouts(m_desc_set_layout);
    layout_info.setPushConstantRanges(push_range);
    m_pipeline_layout = device.createPipelineLayout(layout_info);

    std::array<vk::DescriptorPoolSize, 1> pool_sizes = {
        vk::DescriptorPoolSize{
            .type = vk::DescriptorType::eCombinedImageSampler,
            .descriptorCount = 512,
        }
    };
    vk::DescriptorPoolCreateInfo pool_info{
        .maxSets = 256,
    };
    pool_info.setPoolSizes(pool_sizes);
    ensure_frame_resources(std::max<uint32_t>(state.screen_renderer.swapchain_size, 1));

    vk::SamplerCreateInfo sampler_info{
        .magFilter = vk::Filter::eLinear,
        .minFilter = vk::Filter::eLinear,
        .mipmapMode = vk::SamplerMipmapMode::eNearest,
        .addressModeU = vk::SamplerAddressMode::eClampToEdge,
        .addressModeV = vk::SamplerAddressMode::eClampToEdge,
        .addressModeW = vk::SamplerAddressMode::eClampToEdge,
    };
    m_font_sampler = device.createSampler(sampler_info);

    sampler_info.mipmapMode = vk::SamplerMipmapMode::eLinear;
    sampler_info.maxLod = 1000.0f;
    m_image_sampler = device.createSampler(sampler_info);

    m_dummy_texture = vkutil::Image(1, 1, vk::Format::eR8G8B8A8Unorm);
    m_dummy_texture.init_image(vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst);

    {
        vk::CommandBuffer cmd = vkutil::create_single_time_command(device, state.general_command_pool);
        m_dummy_texture.transition_to_discard(cmd, vkutil::ImageLayout::TransferDst);

        const uint32_t white_pixel = 0xFFFFFFFF;
        vkutil::Buffer staging(4);
        staging.init_buffer(vk::BufferUsageFlagBits::eTransferSrc, vkutil::vma_mapped_alloc);
        memcpy(staging.mapped_data, &white_pixel, 4);

        vk::BufferImageCopy region{
            .bufferOffset = 0,
            .bufferRowLength = 1,
            .bufferImageHeight = 1,
            .imageSubresource = vkutil::color_subresource_layer,
            .imageOffset = { 0, 0, 0 },
            .imageExtent = { 1, 1, 1 },
        };
        cmd.copyBufferToImage(staging.buffer, m_dummy_texture.image, vk::ImageLayout::eTransferDstOptimal, region);
        m_dummy_texture.transition_to(cmd, vkutil::ImageLayout::SampledImage);

        vkutil::end_single_time_command(device, state.general_queue, state.general_command_pool, cmd);
        staging.destroy();
    }

    {
        vk::ImageCreateInfo array_img_info{
            .imageType = vk::ImageType::e2D,
            .format = vk::Format::eR8G8B8A8Unorm,
            .extent = { 1, 1, 1 },
            .mipLevels = 1,
            .arrayLayers = 1,
            .samples = vk::SampleCountFlagBits::e1,
            .tiling = vk::ImageTiling::eOptimal,
            .usage = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst,
            .sharingMode = vk::SharingMode::eExclusive,
            .initialLayout = vk::ImageLayout::eUndefined,
        };
        vma::AllocationCreateInfo alloc_create = vkutil::vma_auto_alloc;
        auto [image, allocation] = m_state->allocator.createImage(array_img_info, alloc_create);
        m_dummy_texture_array.image = image;
        m_dummy_texture_array.allocation = allocation;
        m_dummy_texture_array.width = 1;
        m_dummy_texture_array.height = 1;
        m_dummy_texture_array.format = vk::Format::eR8G8B8A8Unorm;
        m_dummy_texture_array.layout = vkutil::ImageLayout::Undefined;

        vk::ImageViewCreateInfo view_info{
            .image = m_dummy_texture_array.image,
            .viewType = vk::ImageViewType::e2DArray,
            .format = vk::Format::eR8G8B8A8Unorm,
            .components = {
                vk::ComponentSwizzle::eIdentity,
                vk::ComponentSwizzle::eIdentity,
                vk::ComponentSwizzle::eIdentity,
                vk::ComponentSwizzle::eIdentity },
            .subresourceRange = {
                .aspectMask = vk::ImageAspectFlagBits::eColor,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1,
            },
        };
        m_dummy_array_view = device.createImageView(view_info);

        vk::CommandBuffer cmd = vkutil::create_single_time_command(device, state.general_command_pool);
        vk::ImageSubresourceRange range{
            .aspectMask = vk::ImageAspectFlagBits::eColor,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1,
        };
        vkutil::transition_image_layout_discard(cmd, m_dummy_texture_array.image,
            m_dummy_texture_array.layout, vkutil::ImageLayout::TransferDst, range);

        const uint32_t white_pixel = 0xFFFFFFFF;
        vkutil::Buffer staging(4);
        staging.init_buffer(vk::BufferUsageFlagBits::eTransferSrc, vkutil::vma_mapped_alloc);
        memcpy(staging.mapped_data, &white_pixel, 4);

        vk::BufferImageCopy region{
            .bufferOffset = 0,
            .bufferRowLength = 1,
            .bufferImageHeight = 1,
            .imageSubresource = {
                .aspectMask = vk::ImageAspectFlagBits::eColor,
                .mipLevel = 0,
                .baseArrayLayer = 0,
                .layerCount = 1,
            },
            .imageOffset = { 0, 0, 0 },
            .imageExtent = { 1, 1, 1 },
        };
        cmd.copyBufferToImage(staging.buffer, m_dummy_texture_array.image,
            vk::ImageLayout::eTransferDstOptimal, region);
        vkutil::transition_image_layout(cmd, m_dummy_texture_array.image,
            vkutil::ImageLayout::TransferDst, vkutil::ImageLayout::SampledImage, range);
        m_dummy_texture_array.layout = vkutil::ImageLayout::SampledImage;

        vkutil::end_single_time_command(device, state.general_queue, state.general_command_pool, cmd);
        staging.destroy();
    }

    return true;
}

void OverlayRenderer::ensure_frame_resources(uint32_t frame_count) {
    if (!m_state || m_desc_pools.size() >= frame_count)
        return;

    vk::Device device = m_state->device;
    std::array<vk::DescriptorPoolSize, 1> pool_sizes = {
        vk::DescriptorPoolSize{
            .type = vk::DescriptorType::eCombinedImageSampler,
            .descriptorCount = 512,
        }
    };
    vk::DescriptorPoolCreateInfo pool_info{
        .maxSets = 256,
    };
    pool_info.setPoolSizes(pool_sizes);

    const size_t previous_size = m_desc_pools.size();
    m_desc_pools.resize(frame_count);
    m_vertex_buffers.resize(frame_count);
    m_staging_garbage.resize(frame_count);

    for (size_t index = previous_size; index < frame_count; ++index) {
        m_desc_pools[index] = device.createDescriptorPool(pool_info);
    }
}

void OverlayRenderer::create_pipeline(vk::RenderPass render_pass) {
    if (m_pipelines[0]) {
        if (m_current_render_pass == render_pass)
            return;
        destroy_pipeline();
    }

    vk::Device device = m_state->device;

    vk::PipelineShaderStageCreateInfo vert_info{
        .stage = vk::ShaderStageFlagBits::eVertex,
        .module = m_vert_shader,
        .pName = "main"
    };
    vk::PipelineShaderStageCreateInfo frag_info{
        .stage = vk::ShaderStageFlagBits::eFragment,
        .module = m_frag_shader,
        .pName = "main"
    };
    std::array<vk::PipelineShaderStageCreateInfo, 2> shader_stages = { vert_info, frag_info };

    vk::VertexInputBindingDescription binding_descr{
        .binding = 0,
        .stride = sizeof(overlay::vertex),
        .inputRate = vk::VertexInputRate::eVertex
    };
    vk::VertexInputAttributeDescription attr_descr{
        .location = 0,
        .binding = 0,
        .format = vk::Format::eR32G32B32A32Sfloat,
        .offset = 0
    };
    vk::PipelineVertexInputStateCreateInfo vertex_input{};
    vertex_input.setVertexBindingDescriptions(binding_descr);
    vertex_input.setVertexAttributeDescriptions(attr_descr);

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
        .blendEnable = VK_TRUE,
        .srcColorBlendFactor = vk::BlendFactor::eSrcAlpha,
        .dstColorBlendFactor = vk::BlendFactor::eOneMinusSrcAlpha,
        .colorBlendOp = vk::BlendOp::eAdd,
        .srcAlphaBlendFactor = vk::BlendFactor::eZero,
        .dstAlphaBlendFactor = vk::BlendFactor::eOne,
        .alphaBlendOp = vk::BlendOp::eAdd,
        .colorWriteMask = vkutil::default_color_mask,
    };
    vk::PipelineColorBlendStateCreateInfo color_blending{};
    color_blending.setAttachments(blend_attachment);

    vk::PipelineDepthStencilStateCreateInfo depth_stencil{
        .depthTestEnable = VK_FALSE,
        .depthWriteEnable = VK_FALSE,
    };

    static vk::DynamicState dynamic_states[] = {
        vk::DynamicState::eViewport,
        vk::DynamicState::eScissor
    };
    vk::PipelineDynamicStateCreateInfo dynamic_info{};
    dynamic_info.setDynamicStates(dynamic_states);

    static constexpr vk::PrimitiveTopology topologies[NUM_TOPOLOGIES] = {
        vk::PrimitiveTopology::eTriangleStrip,
        vk::PrimitiveTopology::eLineList,
        vk::PrimitiveTopology::eLineStrip,
        vk::PrimitiveTopology::eTriangleFan,
    };

    for (int i = 0; i < NUM_TOPOLOGIES; ++i) {
        vk::PipelineInputAssemblyStateCreateInfo input_assembly{
            .topology = topologies[i]
        };

        vk::GraphicsPipelineCreateInfo pipeline_info{
            .pVertexInputState = &vertex_input,
            .pInputAssemblyState = &input_assembly,
            .pViewportState = &viewport_state,
            .pRasterizationState = &rasterizer,
            .pMultisampleState = &multisampling,
            .pDepthStencilState = &depth_stencil,
            .pColorBlendState = &color_blending,
            .pDynamicState = &dynamic_info,
            .layout = m_pipeline_layout,
            .renderPass = render_pass,
            .subpass = 0
        };
        pipeline_info.setStages(shader_stages);

        const auto result = device.createGraphicsPipeline(VK_NULL_HANDLE, pipeline_info);
        if (result.result != vk::Result::eSuccess) {
            LOG_ERROR("Failed to create overlay Vulkan pipeline (topology {})", i);
            continue;
        }
        m_pipelines[i] = result.value;
    }

    m_current_render_pass = render_pass;
}

void OverlayRenderer::destroy_pipeline() {
    if (!m_state)
        return;
    vk::Device device = m_state->device;
    for (auto &p : m_pipelines) {
        if (p) {
            device.destroy(p);
            p = nullptr;
        }
    }
    m_current_render_pass = nullptr;
}

int OverlayRenderer::pipeline_index(overlay::primitive_type t) {
    switch (t) {
    case overlay::primitive_type::quad_list:
    case overlay::primitive_type::triangle_strip: return 0;
    case overlay::primitive_type::line_list: return 1;
    case overlay::primitive_type::line_strip: return 2;
    case overlay::primitive_type::triangle_fan: return 3;
    default: return 0;
    }
}

void OverlayRenderer::destroy() {
    if (!m_state)
        return;

    vk::Device device = m_state->device;
    device.waitIdle();

    destroy_pipeline();

    for (auto &pool : m_desc_pools) {
        if (pool) {
            device.destroy(pool);
            pool = nullptr;
        }
    }
    m_desc_pools.clear();

    device.destroy(m_pipeline_layout);
    m_pipeline_layout = nullptr;
    device.destroy(m_desc_set_layout);
    m_desc_set_layout = nullptr;

    device.destroy(m_vert_shader);
    m_vert_shader = nullptr;
    device.destroy(m_frag_shader);
    m_frag_shader = nullptr;

    device.destroy(m_font_sampler);
    m_font_sampler = nullptr;
    device.destroy(m_image_sampler);
    m_image_sampler = nullptr;

    for (auto &[ptr, entry] : m_font_atlas_cache)
        entry.image.destroy();
    m_font_atlas_cache.clear();
    m_dummy_texture.destroy();
    if (m_dummy_array_view) {
        device.destroy(m_dummy_array_view);
        m_dummy_array_view = nullptr;
    }
    m_dummy_texture_array.destroy();

    for (auto &vb : m_vertex_buffers)
        vb.destroy();
    m_vertex_buffers.clear();

    for (auto &garbage : m_staging_garbage) {
        for (auto &buf : garbage)
            buf.destroy();
        garbage.clear();
    }
    m_staging_garbage.clear();

    for (auto &img : m_icon_images)
        img.destroy();
    m_icon_images.clear();

    for (auto &[ptr, img] : m_raw_image_cache)
        img.destroy();
    m_raw_image_cache.clear();

    m_prepared_views.clear();

    m_resources.free_resources();
    m_resources_loaded = false;

    m_state = nullptr;
}

vk::ImageView OverlayRenderer::upload_font(vk::CommandBuffer cmd, const overlay::font *f) {
    if (!f || !m_state)
        return {};

    const auto dims = f->get_glyph_data_dimensions();
    if (dims.depth == 0)
        return {};

    auto &entry = m_font_atlas_cache[f];

    if (entry.image.image && entry.layers == dims.depth)
        return entry.image.view;

    const auto &glyph_data = f->get_glyph_data();
    if (glyph_data.empty())
        return {};

    vk::Device device = m_state->device;

    entry.image.destroy();

    vk::ImageCreateInfo image_info{
        .imageType = vk::ImageType::e2D,
        .format = vk::Format::eR8Unorm,
        .extent = { dims.width, dims.height, 1 },
        .mipLevels = 1,
        .arrayLayers = dims.depth,
        .samples = vk::SampleCountFlagBits::e1,
        .tiling = vk::ImageTiling::eOptimal,
        .usage = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst,
        .sharingMode = vk::SharingMode::eExclusive,
        .initialLayout = vk::ImageLayout::eUndefined,
    };

    vma::AllocationCreateInfo alloc_create = vkutil::vma_auto_alloc;
    auto [image, allocation] = m_state->allocator.createImage(image_info, alloc_create);
    entry.image.image = image;
    entry.image.allocation = allocation;
    entry.image.width = dims.width;
    entry.image.height = dims.height;
    entry.image.format = vk::Format::eR8Unorm;
    entry.image.layout = vkutil::ImageLayout::Undefined;

    vk::ImageViewCreateInfo view_info{
        .image = entry.image.image,
        .viewType = vk::ImageViewType::e2DArray,
        .format = vk::Format::eR8Unorm,
        .components = {
            vk::ComponentSwizzle::eR,
            vk::ComponentSwizzle::eR,
            vk::ComponentSwizzle::eR,
            vk::ComponentSwizzle::eR },
        .subresourceRange = {
            .aspectMask = vk::ImageAspectFlagBits::eColor,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = dims.depth,
        }
    };
    entry.image.view = device.createImageView(view_info);
    entry.layers = dims.depth;

    const size_t page_size = static_cast<size_t>(dims.width) * dims.height;
    const size_t total_size = page_size * dims.depth;

    vkutil::Buffer staging(total_size);
    staging.init_buffer(vk::BufferUsageFlagBits::eTransferSrc, vkutil::vma_mapped_alloc);
    memcpy(staging.mapped_data, glyph_data.data(), total_size);

    vk::ImageSubresourceRange range{
        .aspectMask = vk::ImageAspectFlagBits::eColor,
        .baseMipLevel = 0,
        .levelCount = 1,
        .baseArrayLayer = 0,
        .layerCount = dims.depth,
    };
    vkutil::transition_image_layout_discard(cmd, entry.image.image,
        entry.image.layout, vkutil::ImageLayout::TransferDst, range);

    std::vector<vk::BufferImageCopy> copies(dims.depth);
    for (uint32_t layer = 0; layer < dims.depth; ++layer) {
        copies[layer] = vk::BufferImageCopy{
            .bufferOffset = layer * page_size,
            .bufferRowLength = dims.width,
            .bufferImageHeight = dims.height,
            .imageSubresource = {
                .aspectMask = vk::ImageAspectFlagBits::eColor,
                .mipLevel = 0,
                .baseArrayLayer = layer,
                .layerCount = 1,
            },
            .imageOffset = { 0, 0, 0 },
            .imageExtent = { dims.width, dims.height, 1 },
        };
    }
    cmd.copyBufferToImage(staging.buffer, entry.image.image,
        vk::ImageLayout::eTransferDstOptimal, copies);

    vkutil::transition_image_layout(cmd, entry.image.image,
        vkutil::ImageLayout::TransferDst, vkutil::ImageLayout::SampledImage, range);
    entry.image.layout = vkutil::ImageLayout::SampledImage;

    m_staging_garbage[m_active_frame_slot].push_back(std::move(staging));

    return entry.image.view;
}

void OverlayRenderer::upload_image(vk::CommandBuffer cmd, const overlay::image_info_base *img, vkutil::Image &dst) {
    if (!img || !img->get_data() || !m_state)
        return;

    vk::Device device = m_state->device;

    const vk::Format fmt = (img->channels == 4) ? vk::Format::eR8G8B8A8Unorm : vk::Format::eR8Unorm;
    const uint32_t w = static_cast<uint32_t>(img->w);
    const uint32_t h = static_cast<uint32_t>(img->h);
    const bool need_mipmaps = std::max(w, h) > MIPMAP_SIZE_THRESHOLD;
    const uint32_t mip_levels = need_mipmaps
        ? static_cast<uint32_t>(std::floor(std::log2(std::max(w, h)))) + 1
        : 1;

    if (dst.image && (dst.width != w || dst.height != h || dst.format != fmt)) {
        dst.destroy();
    }

    if (!dst.image) {
        vk::ImageUsageFlags usage = vk::ImageUsageFlagBits::eSampled
            | vk::ImageUsageFlagBits::eTransferDst;
        if (need_mipmaps)
            usage |= vk::ImageUsageFlagBits::eTransferSrc;

        vk::ImageCreateInfo image_info{
            .imageType = vk::ImageType::e2D,
            .format = fmt,
            .extent = { w, h, 1 },
            .mipLevels = mip_levels,
            .arrayLayers = 1,
            .samples = vk::SampleCountFlagBits::e1,
            .tiling = vk::ImageTiling::eOptimal,
            .usage = usage,
            .sharingMode = vk::SharingMode::eExclusive,
            .initialLayout = vk::ImageLayout::eUndefined,
        };

        vma::AllocationCreateInfo alloc_create = vkutil::vma_auto_alloc;
        auto [image, allocation] = m_state->allocator.createImage(image_info, alloc_create);
        dst.image = image;
        dst.allocation = allocation;
        dst.width = w;
        dst.height = h;
        dst.format = fmt;
        dst.layout = vkutil::ImageLayout::Undefined;

        vk::ImageViewCreateInfo view_info{
            .image = dst.image,
            .viewType = vk::ImageViewType::e2D,
            .format = fmt,
            .components = vkutil::default_comp_mapping,
            .subresourceRange = {
                .aspectMask = vk::ImageAspectFlagBits::eColor,
                .baseMipLevel = 0,
                .levelCount = mip_levels,
                .baseArrayLayer = 0,
                .layerCount = 1,
            },
        };
        dst.view = device.createImageView(view_info);
    }

    const size_t data_size = static_cast<size_t>(w) * h * img->channels;

    vkutil::Buffer staging(data_size);
    staging.init_buffer(vk::BufferUsageFlagBits::eTransferSrc, vkutil::vma_mapped_alloc);
    memcpy(staging.mapped_data, img->get_data(), data_size);

    vk::ImageSubresourceRange all_mips{
        .aspectMask = vk::ImageAspectFlagBits::eColor,
        .baseMipLevel = 0,
        .levelCount = mip_levels,
        .baseArrayLayer = 0,
        .layerCount = 1,
    };
    vkutil::transition_image_layout_discard(cmd, dst.image,
        dst.layout, vkutil::ImageLayout::TransferDst, all_mips);

    vk::BufferImageCopy region{
        .bufferOffset = 0,
        .bufferRowLength = w,
        .bufferImageHeight = h,
        .imageSubresource = vkutil::color_subresource_layer,
        .imageOffset = { 0, 0, 0 },
        .imageExtent = { w, h, 1 },
    };
    cmd.copyBufferToImage(staging.buffer, dst.image,
        vk::ImageLayout::eTransferDstOptimal, region);

    if (need_mipmaps) {
        uint32_t mip_w = w, mip_h = h;
        for (uint32_t i = 1; i < mip_levels; ++i) {
            vk::ImageSubresourceRange src_range{
                .aspectMask = vk::ImageAspectFlagBits::eColor,
                .baseMipLevel = i - 1,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1,
            };
            vkutil::transition_image_layout(cmd, dst.image,
                vkutil::ImageLayout::TransferDst, vkutil::ImageLayout::TransferSrc, src_range);

            uint32_t next_w = std::max(mip_w / 2, 1u);
            uint32_t next_h = std::max(mip_h / 2, 1u);

            vk::ImageBlit blit{
                .srcSubresource = {
                    .aspectMask = vk::ImageAspectFlagBits::eColor,
                    .mipLevel = i - 1,
                    .baseArrayLayer = 0,
                    .layerCount = 1,
                },
                .dstSubresource = {
                    .aspectMask = vk::ImageAspectFlagBits::eColor,
                    .mipLevel = i,
                    .baseArrayLayer = 0,
                    .layerCount = 1,
                },
            };
            blit.srcOffsets[0] = vk::Offset3D{ 0, 0, 0 };
            blit.srcOffsets[1] = vk::Offset3D{ static_cast<int32_t>(mip_w), static_cast<int32_t>(mip_h), 1 };
            blit.dstOffsets[0] = vk::Offset3D{ 0, 0, 0 };
            blit.dstOffsets[1] = vk::Offset3D{ static_cast<int32_t>(next_w), static_cast<int32_t>(next_h), 1 };

            cmd.blitImage(dst.image, vk::ImageLayout::eTransferSrcOptimal,
                dst.image, vk::ImageLayout::eTransferDstOptimal,
                blit, vk::Filter::eLinear);

            mip_w = next_w;
            mip_h = next_h;
        }

        vk::ImageSubresourceRange src_levels{
            .aspectMask = vk::ImageAspectFlagBits::eColor,
            .baseMipLevel = 0,
            .levelCount = mip_levels - 1,
            .baseArrayLayer = 0,
            .layerCount = 1,
        };
        vkutil::transition_image_layout(cmd, dst.image,
            vkutil::ImageLayout::TransferSrc, vkutil::ImageLayout::SampledImage, src_levels);

        vk::ImageSubresourceRange last_level{
            .aspectMask = vk::ImageAspectFlagBits::eColor,
            .baseMipLevel = mip_levels - 1,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1,
        };
        vkutil::transition_image_layout(cmd, dst.image,
            vkutil::ImageLayout::TransferDst, vkutil::ImageLayout::SampledImage, last_level);
    } else {
        vkutil::transition_image_layout(cmd, dst.image,
            vkutil::ImageLayout::TransferDst, vkutil::ImageLayout::SampledImage, all_mips);
    }

    dst.layout = vkutil::ImageLayout::SampledImage;

    m_staging_garbage[m_active_frame_slot].push_back(std::move(staging));

    img->dirty = false;
}

void OverlayRenderer::update_descriptor_set(vk::DescriptorSet set,
    vk::ImageView tex_2d, vk::ImageView tex_array) {
    vk::DescriptorImageInfo img_2d_info{
        .sampler = m_image_sampler,
        .imageView = tex_2d ? tex_2d : m_dummy_texture.view,
        .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
    };
    vk::DescriptorImageInfo img_array_info{
        .sampler = m_font_sampler,
        .imageView = tex_array ? tex_array : m_dummy_array_view,
        .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
    };

    std::array<vk::WriteDescriptorSet, 2> writes = {
        vk::WriteDescriptorSet{
            .dstSet = set,
            .dstBinding = 0,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = vk::DescriptorType::eCombinedImageSampler,
            .pImageInfo = &img_2d_info,
        },
        vk::WriteDescriptorSet{
            .dstSet = set,
            .dstBinding = 1,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = vk::DescriptorType::eCombinedImageSampler,
            .pImageInfo = &img_array_info,
        }
    };

    m_state->device.updateDescriptorSets(writes, {});
}

vk::DescriptorSet OverlayRenderer::allocate_descriptor_set() {
    vk::DescriptorSetAllocateInfo alloc_info{
        .descriptorPool = m_desc_pools[m_active_frame_slot],
    };
    alloc_info.setSetLayouts(m_desc_set_layout);
    auto sets = m_state->device.allocateDescriptorSets(alloc_info);
    return sets.front();
}

void OverlayRenderer::draw_command(vk::CommandBuffer cmd,
    const overlay::compiled_resource::command &draw_cmd,
    float viewport_w, float viewport_h,
    float viewport_x, float viewport_y) {
    if (draw_cmd.verts.empty())
        return;

    const auto &config = draw_cmd.config;

    const vk::DeviceSize data_size = draw_cmd.verts.size() * sizeof(overlay::vertex);
    const vk::DeviceSize aligned_size = (data_size + 15) & ~vk::DeviceSize(15);
    const vk::DeviceSize required = m_vertex_buffer_offset + aligned_size;

    auto &vb = m_vertex_buffers[m_active_frame_slot];
    if (!vb.buffer || required > vb.size) {
        vb.destroy();
        vb = vkutil::Buffer(std::max(required * 2, vk::DeviceSize(256 * 1024)));
        vb.init_buffer(
            vk::BufferUsageFlagBits::eVertexBuffer,
            vkutil::vma_mapped_alloc);
        m_vertex_buffer_offset = 0;
    }

    memcpy(static_cast<uint8_t *>(vb.mapped_data) + m_vertex_buffer_offset,
        draw_cmd.verts.data(), data_size);
    const vk::DeviceSize this_offset = m_vertex_buffer_offset;
    m_vertex_buffer_offset += aligned_size;

    vk::ImageView tex_2d_view;
    vk::ImageView tex_array_view;

    const uint8_t ref = config.texture_ref;
    if (ref == overlay::image_resource_none
        || ref == overlay::game_icon
        || ref == overlay::backbuffer) {
    } else if (config.font_ref) {
        auto it = m_font_atlas_cache.find(config.font_ref);
        if (it != m_font_atlas_cache.end() && it->second.image.view)
            tex_array_view = it->second.image.view;
    } else if (ref == overlay::raw_image && config.external_data_ref) {
        auto it = m_raw_image_cache.find(config.external_data_ref);
        if (it != m_raw_image_cache.end() && it->second.view)
            tex_2d_view = it->second.view;
    } else {
        const size_t idx = static_cast<size_t>(ref - 1);
        if (idx < m_icon_images.size() && m_icon_images[idx].view)
            tex_2d_view = m_icon_images[idx].view;
    }

    vk::DescriptorSet desc_set = allocate_descriptor_set();
    update_descriptor_set(desc_set, tex_2d_view, tex_array_view);
    cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipeline_layout, 0, desc_set, {});

    OverlayPushConstants pc{};

    pc.ui_scale[0] = VIRTUAL_WIDTH;
    pc.ui_scale[1] = VIRTUAL_HEIGHT;
    pc.ui_scale[2] = 1.f;
    pc.ui_scale[3] = 1.f;

    pc.albedo[0] = config.color.r;
    pc.albedo[1] = config.color.g;
    pc.albedo[2] = config.color.b;
    pc.albedo[3] = config.color.a;

    pc.viewport[0] = viewport_w;
    pc.viewport[1] = viewport_h;
    pc.viewport[2] = viewport_x;
    pc.viewport[3] = viewport_y;

    pc.clip_bounds[0] = config.clip_rect.x1;
    pc.clip_bounds[1] = config.clip_rect.y1;
    pc.clip_bounds[2] = config.clip_rect.x2;
    pc.clip_bounds[3] = config.clip_rect.y2;

    uint32_t vert_cfg = 0;
    if (config.disable_vertex_snap)
        vert_cfg |= 1u;
    pc.vertex_config = vert_cfg;

    uint32_t frag_cfg = 0;
    if (config.clip_region)
        frag_cfg |= 1u;
    if (config.pulse_glow)
        frag_cfg |= 2u;

    uint32_t sampler_mode = 0;
    if (config.font_ref) {
        sampler_mode = 2;
    } else if (config.texture_ref == overlay::font_file) {
        sampler_mode = 1;
    } else if (config.texture_ref != overlay::image_resource_none
        && config.texture_ref != overlay::game_icon
        && config.texture_ref != overlay::backbuffer) {
        sampler_mode = 3;
    }
    frag_cfg |= (sampler_mode & 3u) << 2u;

    const bool is_sdf = config.active_effect == overlay::compiled_resource::effect_type::sdf
        && config.effect.sdf.func != overlay::sdf_function::none;
    const bool is_gloss = config.active_effect == overlay::compiled_resource::effect_type::gloss;
    const bool is_btn_gloss = config.active_effect == overlay::compiled_resource::effect_type::btn_gloss;
    const bool has_sdf_btn_gloss = is_sdf && config.effect.sdf.btn_gloss_height > 0.f;

    uint32_t sdf_type = is_sdf ? static_cast<uint32_t>(config.effect.sdf.func) : 0u;
    frag_cfg |= (sdf_type & 3u) << 4u;
    if (is_gloss)
        frag_cfg |= 64u;
    if (is_btn_gloss || has_sdf_btn_gloss)
        frag_cfg |= 128u;
    pc.fragment_config = frag_cfg;

    pc.timestamp = config.get_sinus_value();

    pc.blur_intensity = static_cast<float>(config.blur_strength);

    // SDF parameters - transform from virtual space to viewport pixel space
    if (is_sdf) {
        auto sdf = config.effect.sdf;
        // Vulkan has Y-down, so no flip needed for the viewport area
        overlay::areaf target_vp;
        target_vp.x1 = viewport_x;
        target_vp.y1 = viewport_y;
        target_vp.x2 = viewport_x + viewport_w;
        target_vp.y2 = viewport_y + viewport_h;
        sdf.transform(target_vp, { VIRTUAL_WIDTH, VIRTUAL_HEIGHT });

        pc.sdf_params[0] = sdf.hx;
        pc.sdf_params[1] = sdf.hy;
        pc.sdf_params[2] = sdf.br;
        pc.sdf_params[3] = sdf.bw;
        pc.sdf_origin[0] = sdf.cx;
        pc.sdf_origin[1] = sdf.cy;
        // Pack btn_gloss params into sdf_origin[2..3] when both SDF and btn_gloss active
        if (has_sdf_btn_gloss) {
            pc.sdf_origin[2] = sdf.btn_gloss_height;
            pc.sdf_origin[3] = std::round(sdf.btn_gloss_opacity * 100.f) + sdf.btn_gloss_bottom_opacity;
        }
        pc.sdf_border_color[0] = sdf.border_color.r;
        pc.sdf_border_color[1] = sdf.border_color.g;
        pc.sdf_border_color[2] = sdf.border_color.b;
        pc.sdf_border_color[3] = sdf.border_color.a;
    } else if (is_gloss) {
        const auto &g = config.effect.gloss;
        pc.sdf_params[0] = g.height;
        pc.sdf_params[1] = g.feather;
        pc.sdf_params[2] = g.opacity;
    } else if (is_btn_gloss) {
        const auto &bg = config.effect.btn_gloss;
        pc.sdf_params[0] = bg.height;
        pc.sdf_params[1] = bg.curve_lift;
        pc.sdf_params[2] = bg.opacity;
        pc.sdf_params[3] = bg.border_radius_frac;
        pc.sdf_origin[0] = bg.aspect;
        pc.sdf_origin[1] = bg.bottom_opacity;
    }

    cmd.pushConstants(m_pipeline_layout,
        vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment,
        0, sizeof(OverlayPushConstants), &pc);

    cmd.bindVertexBuffers(0, vb.buffer, this_offset);

    int prim_idx = pipeline_index(config.primitives);
    if (m_pipelines[prim_idx])
        cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, m_pipelines[prim_idx]);

    switch (config.primitives) {
    case overlay::primitive_type::quad_list: {
        const uint32_t vert_count = static_cast<uint32_t>(draw_cmd.verts.size());
        const uint32_t num_quads = vert_count / 4;
        for (uint32_t i = 0; i < num_quads; ++i) {
            cmd.draw(4, 1, i * 4, 0);
        }
        break;
    }
    case overlay::primitive_type::triangle_strip:
    case overlay::primitive_type::line_list:
    case overlay::primitive_type::line_strip:
    case overlay::primitive_type::triangle_fan:
        cmd.draw(static_cast<uint32_t>(draw_cmd.verts.size()), 1, 0, 0);
        break;
    }
}

void OverlayRenderer::prepare(vk::CommandBuffer cmd_buffer,
    const overlay::display_manager &manager,
    float viewport_w, float viewport_h,
    uint32_t frame_slot) {
    m_prepared_views.clear();

    if (!m_state)
        return;

    ensure_frame_resources(frame_slot + 1);
    m_active_frame_slot = frame_slot;

    auto &garbage = m_staging_garbage[m_active_frame_slot];
    for (auto &buf : garbage)
        buf.destroy();
    garbage.clear();

    m_state->device.resetDescriptorPool(m_desc_pools[m_active_frame_slot]);

    m_vertex_buffer_offset = 0;

    manager.lock_shared();

    for (const auto &view : manager.get_views()) {
        if (!view || !view->visible.load())
            continue;

        view->set_render_viewport(
            static_cast<uint16_t>(std::min<uint32_t>(static_cast<uint32_t>(viewport_w), UINT16_MAX)),
            static_cast<uint16_t>(std::min<uint32_t>(static_cast<uint32_t>(viewport_h), UINT16_MAX)));

        auto compiled = view->get_compiled();

        for (const auto &draw_cmd : compiled.draw_commands) {
            const auto &config = draw_cmd.config;

            if (config.font_ref) {
                upload_font(cmd_buffer, config.font_ref);
            } else if (config.texture_ref == overlay::raw_image && config.external_data_ref) {
                auto &img = m_raw_image_cache[config.external_data_ref];
                const auto *info = static_cast<const overlay::image_info_base *>(config.external_data_ref);
                if (!img.image || info->dirty)
                    upload_image(cmd_buffer, info, img);
            } else if (config.texture_ref > 0
                && config.texture_ref != overlay::font_file
                && config.texture_ref != overlay::game_icon
                && config.texture_ref != overlay::backbuffer
                && config.texture_ref < overlay::raw_image) {
                if (!m_resources_loaded) {
                    m_resources.load_files();
                    m_resources_loaded = true;
                    m_icon_images.clear();
                }
                const size_t idx = static_cast<size_t>(config.texture_ref - 1);
                if (idx < m_resources.texture_raw_data.size()) {
                    if (m_icon_images.size() <= idx)
                        m_icon_images.resize(idx + 1);
                    auto &icon = m_icon_images[idx];
                    const auto *info = m_resources.texture_raw_data[idx].get();
                    if (info && (!icon.image || info->dirty))
                        upload_image(cmd_buffer, info, icon);
                }
            }
        }

        m_prepared_views.push_back({ view, std::move(compiled) });
    }

    manager.unlock_shared();
}

void OverlayRenderer::render(vk::CommandBuffer cmd_buffer,
    vk::RenderPass render_pass,
    vk::Extent2D extent,
    float viewport_x, float viewport_y,
    float viewport_w, float viewport_h) {
    if (!m_state || m_prepared_views.empty())
        return;

    create_pipeline(render_pass);
    if (!m_pipelines[0])
        return;

    vk::Viewport vk_viewport{
        .x = viewport_x,
        .y = viewport_y,
        .width = viewport_w,
        .height = viewport_h,
        .minDepth = 0.0f,
        .maxDepth = 1.0f,
    };
    cmd_buffer.setViewport(0, vk_viewport);

    vk::Rect2D scissor{
        .offset = { static_cast<int32_t>(viewport_x), static_cast<int32_t>(viewport_y) },
        .extent = { static_cast<uint32_t>(viewport_w), static_cast<uint32_t>(viewport_h) },
    };
    cmd_buffer.setScissor(0, scissor);

    for (auto &prepared : m_prepared_views) {
        for (const auto &draw_cmd : prepared.compiled.draw_commands) {
            draw_command(cmd_buffer, draw_cmd,
                viewport_w, viewport_h, viewport_x, viewport_y);
        }

        const auto timestamp_us = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now().time_since_epoch())
                                      .count();
        prepared.view->update(static_cast<uint64_t>(timestamp_us));
    }

    m_prepared_views.clear();
}

} // namespace renderer::vulkan
