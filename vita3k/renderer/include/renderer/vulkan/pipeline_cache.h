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

#pragma once

#include <blockingconcurrentqueue.h>
#include <util/containers.h>
#include <vkutil/vkutil.h>

#include <array>
#include <limits>
#include <map>
#include <set>

struct SceGxmProgram;
struct SceGxmFragmentProgram;
struct SceGxmVertexProgram;
enum SceGxmPrimitiveType : uint32_t;
struct MemState;

using Sha256Hash = std::array<uint8_t, 32>;

namespace shader {
struct Hints;
}

namespace renderer {

struct GxmRecordState;

namespace vulkan {
struct VKState;
struct VKContext;
struct CompileRequest;

using PipelineCompileQueue = moodycamel::BlockingConcurrentQueue<CompileRequest *>;

class PipelineCache {
    friend struct VKState;

private:
    VKState &state;

    // are we performing pipeline compilation on a parallel thread?
    bool use_async_compilation = false;
    // how many threads are used in case async compilation is enabled
    int nb_worker_threads = 0;

    // does the GPU support vertex attributes with 3 components (like R16G16B16_UNORM), some (like AMD GPUs) don't
    // this is needed when creating the input state
    std::set<vk::Format> unsupported_rgb_vertex_attribute_formats{};
    // does the GPU support SCALED vertex attribute formats (like R8G8_USCALED), some Android GPUs don't
    // we need this information both while creating the pipeline and generating the shader
    bool support_scaled_vertex_attribute;
    // does the GPU support rasterized color attachment (i.e coherent texture fetch)
    // this is needed to properly emulate frag color access in a way similar to opengl framebuffer fetch
    bool support_coherent_framebuffer_fetch;

    // how much time should we wait after the last shader compilation
    // to update the disk-saved shader cache (in seconds)
    static constexpr int pipeline_cache_save_delay = 15;

    vk::PipelineCache pipeline_cache;

    // first index: 0 if color is backed by memory, 1 otherwise
    // second index: 1 if depth-stencil is force loaded, 0 otherwise
    // third index: 1 if depth-stencil is force stored, 0 otherwise
    std::map<vk::Format, vk::RenderPass> render_passes[2][2][2];
    // render passes used along shader interlock
    std::map<vk::Format, vk::RenderPass> shader_interlock_pass;

    // only used when accessing the shaders map
    std::mutex shaders_mutex;
    // because of multithreading, we want the pointers to remain stable
    unordered_map_stable<Sha256Hash, vk::ShaderModule> shaders;
    unordered_map_stable<uint64_t, vk::Pipeline> pipelines;

    vk::PipelineShaderStageCreateInfo retrieve_shader(const SceGxmProgram *program, const Sha256Hash &hash, bool is_vertex, bool maskupdate, MemState &mem, const shader::Hints &hints, bool is_srgb = false);
    vk::PipelineVertexInputStateCreateInfo get_vertex_input_state(const SceGxmVertexProgram &vertex_program, MemState &mem);

    // queue containing request sent by the main thread to the compile threads
    PipelineCompileQueue pipeline_compile_queue;
    moodycamel::ProducerToken pipeline_compile_queue_token;

    // each pipeline compiler thread uses this function as its entrypoint
    void compiler_thread(MemState &mem);

    vk::Pipeline compile_pipeline(SceGxmPrimitiveType type, vk::RenderPass render_pass, const SceGxmVertexProgram &vertex_program_gxm, const SceGxmFragmentProgram &fragment_program_gxm, const GxmRecordState &record, const shader::Hints &hints, MemState &mem);

public:
    // if not 0, next time the pipeline cache should be saved (in seconds since epoch)
    uint64_t next_pipeline_cache_save = std::numeric_limits<uint64_t>::max();

    // modified by the surface cache, estimates if it is safe to use async pipeline compilation
    // (i.e that it does not causes permanent graphical issues)
    bool can_use_deferred_compilation;

    vk::DescriptorSetLayout uniforms_layout;
    // used for the mask, color attachment
    vk::DescriptorSetLayout attachments_layout;
    std::array<vk::DescriptorSetLayout, 17> vertex_textures_layout;
    std::array<vk::DescriptorSetLayout, 17> fragment_textures_layout;

    // there are at most 16 different textures in the fragment shader, and 16 in the vertex shader
    // first index is vertex, second is fragment
    vk::PipelineLayout pipeline_layouts[17][17] = {};

    PipelineCache(VKState &state);
    void init(bool support_rasterized_order_access);

    void read_pipeline_cache();
    void save_pipeline_cache();

    vk::RenderPass retrieve_render_pass(vk::Format format, bool force_load, bool force_store, bool is_color_transient, bool no_color = false);
    vk::Pipeline retrieve_pipeline(VKContext &context, SceGxmPrimitiveType &type, bool consider_for_async, MemState &mem);

    vk::ShaderModule precompile_shader(const Sha256Hash &hash, bool search_first = true);

    void set_async_compilation(bool enable);
};
} // namespace vulkan
} // namespace renderer
