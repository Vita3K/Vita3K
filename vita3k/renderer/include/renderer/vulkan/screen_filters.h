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

#pragma once

#include <vkutil/objects.h>

namespace renderer::vulkan {

struct VKState;
class ScreenRenderer;
struct Viewport;

class ScreenFilter {
protected:
    ScreenRenderer &screen;

public:
    ScreenFilter(ScreenRenderer &screen_renderer);
    virtual void init() = 0;
    virtual void render(bool is_pre_renderpass, vk::ImageView src_img, vk::ImageLayout src_layout, const Viewport &viewport) = 0;
    virtual std::string_view get_name() = 0;
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

} // namespace renderer::vulkan
