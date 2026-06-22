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

#include <overlay/compiled_resource.h>
#include <overlay/controls.h>
#include <overlay/display_manager.h>
#include <vkutil/objects.h>

#include <unordered_map>
#include <vector>

namespace renderer::vulkan {

struct VKState;

struct OverlayPushConstants {
    float ui_scale[4];
    float albedo[4];
    float viewport[4];
    float clip_bounds[4];
    uint32_t vertex_config;
    uint32_t fragment_config;
    float timestamp;
    float blur_intensity;
    float sdf_params[4];
    float sdf_origin[4];
    float sdf_border_color[4];
};
static_assert(sizeof(OverlayPushConstants) == 128);

class OverlayRenderer {
public:
    OverlayRenderer() = default;
    ~OverlayRenderer();

    bool init(VKState &state);
    void destroy();

    void prepare(vk::CommandBuffer cmd_buffer,
        const overlay::display_manager &manager,
        float viewport_w, float viewport_h,
        uint32_t frame_slot);

    void render(vk::CommandBuffer cmd_buffer,
        vk::RenderPass render_pass,
        vk::Extent2D extent,
        float viewport_x, float viewport_y,
        float viewport_w, float viewport_h);

private:
    VKState *m_state = nullptr;

    static constexpr int NUM_TOPOLOGIES = 4;
    vk::Pipeline m_pipelines[NUM_TOPOLOGIES] = {};
    vk::PipelineLayout m_pipeline_layout;
    vk::DescriptorSetLayout m_desc_set_layout;

    std::vector<vk::DescriptorPool> m_desc_pools;
    uint32_t m_active_frame_slot = 0;

    vk::ShaderModule m_vert_shader;
    vk::ShaderModule m_frag_shader;

    struct FontAtlasEntry {
        vkutil::Image image;
        size_t layers = 0;
    };
    std::unordered_map<const overlay::font *, FontAtlasEntry> m_font_atlas_cache;
    vk::Sampler m_font_sampler;

    std::vector<vkutil::Image> m_icon_images;
    std::unordered_map<const void *, vkutil::Image> m_raw_image_cache;
    vk::Sampler m_image_sampler;

    std::vector<vkutil::Buffer> m_vertex_buffers;
    vk::DeviceSize m_vertex_buffer_offset = 0;

    std::vector<std::vector<vkutil::Buffer>> m_staging_garbage;

    vkutil::Image m_dummy_texture;
    vkutil::Image m_dummy_texture_array;
    vk::ImageView m_dummy_array_view;

    overlay::resource_config m_resources;
    bool m_resources_loaded = false;

    vk::RenderPass m_current_render_pass;

    struct PreparedView {
        std::shared_ptr<overlay::overlay> view;
        overlay::compiled_resource compiled;
    };
    std::vector<PreparedView> m_prepared_views;

    static constexpr uint32_t MIPMAP_SIZE_THRESHOLD = 256;

    void ensure_frame_resources(uint32_t frame_count);

    void create_pipeline(vk::RenderPass render_pass);
    void destroy_pipeline();

    vk::ImageView upload_font(vk::CommandBuffer cmd, const overlay::font *f);
    static int pipeline_index(overlay::primitive_type t);
    void upload_image(vk::CommandBuffer cmd, const overlay::image_info_base *img, vkutil::Image &dst);
    vk::DescriptorSet allocate_descriptor_set();
    void update_descriptor_set(vk::DescriptorSet set, vk::ImageView tex_2d, vk::ImageView tex_array);
    void draw_command(vk::CommandBuffer cmd,
        const overlay::compiled_resource::command &draw_cmd,
        float viewport_w, float viewport_h,
        float viewport_x, float viewport_y);
};

} // namespace renderer::vulkan
