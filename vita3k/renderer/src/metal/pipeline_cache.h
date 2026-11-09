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

#include "metal_include.h"

#include <blockingconcurrentqueue.h>
#include <util/containers.h>
#include <util/fs.h>

#include <array>
#include <atomic>
#include <limits>
#include <map>
#include <mutex>
#include <thread>
#include <vector>

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

namespace metal {
struct MetalState;
struct MetalContextImpl;
struct CompileRequest;

using PipelineCompileQueue = moodycamel::BlockingConcurrentQueue<CompileRequest *>;

struct DepthStencilKey {
    uint32_t depth_func;
    uint32_t depth_write;
    uint32_t front_func, front_fail, front_depth_fail, front_pass, front_compare_mask, front_write_mask;
    uint32_t back_func, back_fail, back_depth_fail, back_pass, back_compare_mask, back_write_mask;
    uint32_t stencil_enabled;

    auto operator<=>(const DepthStencilKey &) const = default;
};

struct PipelineEntry {
    id<MTLRenderPipelineState> pipeline = nil;
    // set to true once the pipeline is ready (or failed to compile)
    std::atomic<bool> ready{ false };
};

struct ShaderEntry {
    id<MTLFunction> function = nil;
    std::atomic<bool> compiling{ false };
};

class PipelineCache {
    friend struct MetalState;

private:
    MetalState &state;

    bool use_async_compilation = false;
    int nb_worker_threads = 0;

    static constexpr int pipeline_cache_save_delay = 15;

    std::mutex shaders_mutex;
    unordered_map_stable<Sha256Hash, ShaderEntry> shaders;
    unordered_map_stable<uint64_t, PipelineEntry> pipelines;

    std::map<DepthStencilKey, id<MTLDepthStencilState>> depth_stencil_states;
    id<MTLDepthStencilState> disabled_depth_stencil_state = nil;
    id<MTLFunction> mask_test_function = nil;

    // used to emulate vkCmdClearAttachments on the depth-stencil
    id<MTLLibrary> clear_library = nil;
    std::map<uint32_t, id<MTLRenderPipelineState>> clear_pipelines;
    id<MTLDepthStencilState> clear_depth_stencil_states[4] = {};

    id<MTLFunction> retrieve_shader(const SceGxmProgram *program, const Sha256Hash &hash, bool is_vertex, bool maskupdate, MemState &mem, const shader::Hints &hints);
    id<MTLFunction> compile_function(const std::vector<uint32_t> &spirv, bool is_vertex, const std::string &label, const fs::path &msl_cache_path);
    MTLVertexDescriptor *get_vertex_descriptor(const SceGxmVertexProgram &vertex_program, MemState &mem);

    PipelineCompileQueue pipeline_compile_queue;
    moodycamel::ProducerToken pipeline_compile_queue_token;
    std::vector<std::thread> worker_threads;

    void compiler_thread(MemState &mem);

    id<MTLRenderPipelineState> compile_pipeline(const SceGxmVertexProgram &vertex_program_gxm, const SceGxmFragmentProgram &fragment_program_gxm, const GxmRecordState &record, const shader::Hints &hints, MTLPixelFormat color_format, bool has_depth, MemState &mem);

public:
    uint64_t next_pipeline_cache_save = std::numeric_limits<uint64_t>::max();

    // modified by the surface cache, estimates if it is safe to use async pipeline compilation
    bool can_use_deferred_compilation = false;

    explicit PipelineCache(MetalState &state);
    void init();
    void cleanup();

    void save_pipeline_cache();

    id<MTLRenderPipelineState> retrieve_pipeline(MetalContextImpl &context, bool consider_for_async, MemState &mem);
    id<MTLDepthStencilState> retrieve_depth_stencil_state(const GxmRecordState &record, bool maskupdate);
    id<MTLDepthStencilState> get_disabled_depth_stencil_state();

    id<MTLRenderPipelineState> get_clear_pipeline(MTLPixelFormat color_format, bool has_color);
    id<MTLDepthStencilState> get_clear_depth_stencil_state(bool clear_depth, bool clear_stencil);

    id<MTLFunction> precompile_shader(const Sha256Hash &hash, bool search_first = true);

    void set_async_compilation(bool enable);
};
} // namespace metal
} // namespace renderer
