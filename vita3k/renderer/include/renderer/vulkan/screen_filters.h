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

#pragma once

#include <vkutil/objects.h>

#include <string_view>
#include <vector>

namespace renderer::vulkan {

class ScreenRenderer;
struct Viewport;

class ScreenFilter {
protected:
    ScreenRenderer &screen;

public:
    ScreenFilter(ScreenRenderer &screen_renderer);
    virtual ~ScreenFilter() = default;
    virtual void init() = 0;
    virtual void on_resize() {};
    virtual void render(bool is_pre_renderpass, vk::ImageView src_img, vk::ImageLayout src_layout, const Viewport &viewport) = 0;
    virtual std::string_view get_name() = 0;
    // do we need the render pass not to clear the swapchain content ?
    virtual bool need_post_processing_render_pass() {
        return false;
    }
};

class SinglePassScreenFilter : public ScreenFilter {
private:
    vk::DescriptorSetLayout descriptor_set_layout;
    std::vector<vk::DescriptorSet> descriptor_sets;
    vk::DescriptorPool descriptor_pool;
    vk::PipelineLayout pipeline_layout;
    vk::Pipeline pipeline;

    vkutil::Buffer vao;
    std::vector<std::array<float, 4>> last_uvs;

    vk::ShaderModule vertex_shader;
    vk::ShaderModule fragment_shader;

    vk::Sampler sampler;

    void create_layout_sync();
    void create_graphics_pipeline();

protected:
    // file name inside shaders-builtin/vulkan
    virtual std::string_view get_vertex_name();
    virtual std::string_view get_fragment_name();
    virtual vk::Sampler create_sampler() = 0;

public:
    SinglePassScreenFilter(ScreenRenderer &screen);
    ~SinglePassScreenFilter();
    void init() override;
    void render(bool is_pre_renderpass, vk::ImageView src_img, vk::ImageLayout src_layout, const Viewport &viewport) override;
};

class NearestScreenFilter : public SinglePassScreenFilter {
protected:
    vk::Sampler create_sampler() override;

public:
    NearestScreenFilter(ScreenRenderer &screen)
        : SinglePassScreenFilter(screen) {}

    std::string_view get_name() override {
        return "Nearest";
    }
};

class BilinearScreenFilter : public SinglePassScreenFilter {
protected:
    vk::Sampler create_sampler() override;

public:
    BilinearScreenFilter(ScreenRenderer &screen)
        : SinglePassScreenFilter(screen) {}

    std::string_view get_name() override {
        return "Bilinear";
    }
};

class BicubicScreenFilter : public SinglePassScreenFilter {
protected:
    // the Bicubic filter uses a custom shader
    std::string_view get_fragment_name() override;
    vk::Sampler create_sampler() override;

public:
    BicubicScreenFilter(ScreenRenderer &screen)
        : SinglePassScreenFilter(screen) {}

    std::string_view get_name() override {
        return "Bicubic";
    }
};

class FXAAScreenFilter : public SinglePassScreenFilter {
protected:
    // the FXAA filter uses a custom shader
    std::string_view get_fragment_name() override;
    vk::Sampler create_sampler() override;

public:
    FXAAScreenFilter(ScreenRenderer &screen)
        : SinglePassScreenFilter(screen) {}

    std::string_view get_name() override {
        return "FXAA";
    }
};

class SMAAScreenFilter : public ScreenFilter {
private:
    // offscreen render targets, one (edges, blend) pair per swapchain image,
    // sized to the source texture resolution (lazily (re)created)
    std::vector<vkutil::Image> edges_images;
    std::vector<vkutil::Image> blend_images;
    std::vector<vk::Framebuffer> edges_framebuffers;
    std::vector<vk::Framebuffer> blend_framebuffers;
    uint32_t cached_src_width = 0;
    uint32_t cached_src_height = 0;

    // precomputed SMAA lookup tables (shared, read-only)
    vkutil::Image area_tex;
    vkutil::Image search_tex;

    // render pass used by the edge and blend passes (one RGBA8 color attachment)
    vk::RenderPass offscreen_render_pass;

    // one SPIR-V module per stage, all compiled from SMAA.hlsl
    vk::ShaderModule edge_vertex_shader;
    vk::ShaderModule edge_fragment_shader;
    vk::ShaderModule blend_vertex_shader;
    vk::ShaderModule blend_fragment_shader;
    vk::ShaderModule neighborhood_vertex_shader;
    vk::ShaderModule neighborhood_fragment_shader;

    vk::DescriptorSetLayout edge_set_layout;
    vk::DescriptorSetLayout blend_set_layout;
    vk::DescriptorSetLayout neighborhood_set_layout;
    vk::DescriptorPool descriptor_pool;
    std::vector<vk::DescriptorSet> edge_sets; // per swapchain image
    std::vector<vk::DescriptorSet> blend_sets;
    std::vector<vk::DescriptorSet> neighborhood_sets;

    vk::PipelineLayout edge_pipeline_layout;
    vk::PipelineLayout blend_pipeline_layout;
    vk::PipelineLayout neighborhood_pipeline_layout;
    vk::Pipeline edge_pipeline;
    vk::Pipeline blend_pipeline;
    vk::Pipeline neighborhood_pipeline;

    vk::Sampler linear_sampler;
    vk::Sampler point_sampler;

    // host-visible quad buffer (swapchain_size * 2 quads)
    vkutil::Buffer vao;

    void create_samplers();
    void create_render_pass();
    void load_shaders();
    void create_lut_textures();
    void create_descriptors();
    void create_pipelines();
    vk::Pipeline build_pipeline(vk::ShaderModule vertex_shader, vk::ShaderModule fragment_shader,
        vk::PipelineLayout layout, vk::RenderPass render_pass);
    void ensure_targets(uint32_t width, uint32_t height);
    void destroy_targets();
    void bind_fullscreen_quad(vk::CommandBuffer cmd, bool sub_region, const Viewport &viewport);

public:
    SMAAScreenFilter(ScreenRenderer &screen_renderer)
        : ScreenFilter(screen_renderer) {}
    ~SMAAScreenFilter();
    void init() override;
    void render(bool is_pre_renderpass, vk::ImageView src_img, vk::ImageLayout src_layout, const Viewport &viewport) override;
    std::string_view get_name() override {
        return "SMAA";
    }
    // pass 3 draws into the default (clearing) render pass, like the single-pass
    // filters, so we do NOT need the post-processing render pass.
    // -> leave need_post_processing_render_pass() at its default (false).
};

class FSRScreenFilter : public ScreenFilter {
private:
    // dst of the easu shader, src of the rcas shader
    std::vector<vkutil::Image> intermediate_images;

    vk::ShaderModule easu_shader;
    vk::ShaderModule rcas_shader;

    vk::DescriptorSetLayout descriptor_set_layout;
    vk::DescriptorPool descriptor_pool;
    std::vector<vk::DescriptorSet> descriptor_sets;
    vk::PipelineLayout pipeline_layout_easu;
    vk::PipelineLayout pipeline_layout_rcas;
    vk::Pipeline pipeline_easu;
    vk::Pipeline pipeline_rcas;

    vk::Extent2D output_offset;
    vk::Extent2D output_size;

    vk::Sampler sampler;

public:
    FSRScreenFilter(ScreenRenderer &screen_renderer)
        : ScreenFilter(screen_renderer) {}
    ~FSRScreenFilter();
    void init() override;
    void on_resize() override;
    void render(bool is_pre_renderpass, vk::ImageView src_img, vk::ImageLayout src_layout, const Viewport &viewport) override;
    std::string_view get_name() override {
        return "FSR";
    }
    bool need_post_processing_render_pass() override {
        return true;
    }
};

} // namespace renderer::vulkan
