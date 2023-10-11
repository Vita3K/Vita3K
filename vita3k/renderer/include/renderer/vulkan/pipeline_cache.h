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

#include <array>
#include <limits>
#include <map>
#include <set>

#include <util/containers.h>
#include <vkutil/objects.h>

struct SceGxmProgram;
enum SceGxmPrimitiveType : uint32_t;
struct SceGxmVertexAttribute;
struct MemState;

using Sha256Hash = std::array<uint8_t, 32>;

namespace renderer::vulkan {
struct VKState;
struct VKContext;

class PipelineCache {
private:
    VKState &state;
    VKContext *current_context;

    // does the GPU support vertex attributes with 3 components (like R16G16B16_UNORM), some (like AMD GPUs) don't
    // this is needed when creating the input state
    std::set<vk::Format> unsupported_rgb_vertex_attribute_formats{};

    // how much time should we wait after the last shader compilation
    // to update the disk-saved shader cache (in seconds)
    static constexpr int pipeline_cache_save_delay = 30;

    vk::PipelineCache pipeline_cache;

    // first index: 1 if depth-stencil is force loaded, 0 otherwise
    // second index: 1 if depth-stencil is force stored, 0 otherwise
    std::map<vk::Format, vk::RenderPass> render_passes[2][2];
    // render passes used along shader interlock
    std::map<vk::Format, vk::RenderPass> shader_interlock_pass;
    std::map<Sha256Hash, vk::ShaderModule> shaders;
    unordered_map_fast<uint64_t, vk::Pipeline> pipelines;

    // temp vars used to store the result computed by auxialiary functions before createPipeline is called
    std::vector<vk::VertexInputBindingDescription> binding_descr;
    std::vector<vk::VertexInputAttributeDescription> attr_descr;

    vk::PipelineShaderStageCreateInfo retrieve_shader(const SceGxmProgram *program, const Sha256Hash &hash, bool is_vertex, bool maskupdate, MemState &mem, const std::vector<SceGxmVertexAttribute> *hint_attributes);
    vk::PipelineLayout retrieve_pipeline_layout(const uint16_t vert_texture_count, const uint16_t frag_texture_count);
    vk::PipelineVertexInputStateCreateInfo get_vertex_input_state(MemState &mem);

public:
    // if not 0, next time the pipeline cache should be saved (in seconds since epoch)
    uint64_t next_pipeline_cache_save = std::numeric_limits<uint64_t>::max();

    vk::DescriptorSetLayout uniforms_layout;
    // used for the mask, color attachment
    vk::DescriptorSetLayout attachments_layout;
    std::array<vk::DescriptorSetLayout, 17> vertex_textures_layout;
    std::array<vk::DescriptorSetLayout, 17> fragment_textures_layout;

    // there are at most 16 different textures in the fragment shader, and 16 in the vertex shader
    // first index is vertex, second is fragment
    vk::PipelineLayout pipeline_layouts[17][17] = {};

    explicit PipelineCache(VKState &state);
    void init();

    void read_pipeline_cache();
    void save_pipeline_cache();

    vk::RenderPass retrieve_render_pass(vk::Format format, bool force_load, bool force_store, bool no_color = false);
    vk::Pipeline retrieve_pipeline(VKContext &context, SceGxmPrimitiveType &type, MemState &mem);

    bool precompile_shader(const Sha256Hash &hash);
};
} // namespace renderer::vulkan