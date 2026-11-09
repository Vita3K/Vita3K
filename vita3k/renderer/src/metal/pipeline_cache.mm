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

#include "pipeline_cache.h"

#include "gxm_to_metal.h"
#include "mtl_types.h"
#include "shader_translate.h"

#include <gxm/functions.h>
#include <gxm/types.h>
#include <renderer/shaders.h>
#include <shader/spirv_recompiler.h>

#include <util/fs.h>
#include <util/log.h>

#include <SDL3/SDL_cpuinfo.h>

#define XXH_INLINE_ALL
#include <xxhash.h>

#include <algorithm>
#include <chrono>
#include <fstream>

namespace renderer::metal {

constexpr size_t record_pipeline_len = offsetof(GxmRecordState, vertex_streams);

struct CompileRequest {
    PipelineEntry *entry;

    // this is everything we need to compile the shader on another thread (as the original data will change)
    SceGxmVertexProgram *vertex_program_gxm;
    SceGxmFragmentProgram *fragment_program_gxm;
    shader::Hints hints;
    MTLPixelFormat color_format;
    bool has_depth;

    // the content of the record useful for the pipeline creation
    alignas(8) uint8_t record_data[record_pipeline_len];

    const GxmRecordState *get_record() {
        // note: this object is only half defined, but we are only looking at the part that's defined
        return reinterpret_cast<const GxmRecordState *>(record_data);
    }
};

static const char *clear_shader_source = R"(
#include <metal_stdlib>
using namespace metal;

struct ClearOut {
    float4 position [[position]];
};

vertex ClearOut clear_vs(uint vid [[vertex_id]], constant float &depth [[buffer(0)]]) {
    const float2 pos = float2((vid & 1) ? 1.0 : -1.0, (vid & 2) ? 1.0 : -1.0);
    ClearOut out;
    out.position = float4(pos, depth, 1.0);
    return out;
}

fragment void clear_fs() {
}
)";

PipelineCache::PipelineCache(MetalState &state)
    : state(state)
    , pipeline_compile_queue_token(pipeline_compile_queue) {
}

void PipelineCache::init() {
    @autoreleasepool {
        NSError *error = nil;
        clear_library = [state.device->device newLibraryWithSource:[NSString stringWithUTF8String:clear_shader_source] options:nil error:&error];
        if (!clear_library)
            LOG_ERROR("Failed to compile the Metal clear shader: {}", error ? error.localizedDescription.UTF8String : "unknown error");
    }

    const int nb_logical_threads = SDL_GetNumLogicalCPUCores();
    // took this from RPCS3 (slightly modified)
    if (nb_logical_threads > 12)
        nb_worker_threads = 6;
    else if (nb_logical_threads > 8)
        nb_worker_threads = 4;
    else if (nb_logical_threads >= 8)
        nb_worker_threads = 3;
    else if (nb_logical_threads >= 6)
        nb_worker_threads = 2;
    else
        nb_worker_threads = 1;

    if (use_async_compilation) {
        // we could not initialize the worker threads previously
        use_async_compilation = false;
        set_async_compilation(true);
    }
}

void PipelineCache::set_async_compilation(bool enable) {
    if (enable == use_async_compilation)
        return;

    use_async_compilation = enable;
    if (nb_worker_threads == 0)
        // not ingame yet
        return;

    if (enable) {
        LOG_INFO("Enabling asynchronous pipeline compilation with {} threads", nb_worker_threads);
        worker_threads.reserve(nb_worker_threads);
        for (int i = 0; i < nb_worker_threads; i++) {
            worker_threads.emplace_back(&PipelineCache::compiler_thread, this, std::ref(*state.mem));
        }
    } else {
        LOG_INFO("Asynchronous pipeline compilation is now disabled");

        for (size_t i = 0; i < worker_threads.size(); i++)
            // if a thread receives nullptr, it exits
            pipeline_compile_queue.enqueue(nullptr);

        for (auto &thread : worker_threads) {
            if (thread.joinable())
                thread.join();
        }
        worker_threads.clear();
    }
}

void PipelineCache::save_pipeline_cache() {
    // do a copy for thread safety
    std::vector<ShadersHash> shader_cache_copy;
    {
        std::lock_guard<std::mutex> guard(shaders_mutex);
        shader_cache_copy = state.shaders_cache_hashs;
    }
    renderer::save_shaders_cache_hashs(state, shader_cache_copy);
}

void PipelineCache::cleanup() {
    // stop threads
    if (use_async_compilation)
        set_async_compilation(false);

    pipelines.clear();

    {
        std::lock_guard<std::mutex> guard(shaders_mutex);
        shaders.clear();
        mask_test_function = nil;
    }

    depth_stencil_states.clear();
    disabled_depth_stencil_state = nil;
    clear_pipelines.clear();
    for (auto &ds_state : clear_depth_stencil_states)
        ds_state = nil;
    clear_library = nil;

    next_pipeline_cache_save = std::numeric_limits<uint64_t>::max();
    nb_worker_threads = 0;
}

// return true if the hash is in the manifest, is_vertex is set according to the manifest entry
static bool manifest_lists(MetalState &state, std::mutex &shaders_mutex, const Sha256Hash &hash, bool *is_vertex = nullptr) {
    std::lock_guard<std::mutex> guard(shaders_mutex);
    for (const ShadersHash &entry : state.shaders_cache_hashs) {
        if (entry.vert == hash) {
            if (is_vertex)
                *is_vertex = true;
            return true;
        }
        if (entry.frag == hash) {
            if (is_vertex)
                *is_vertex = false;
            return true;
        }
    }
    return false;
}

static std::string read_text_file(const fs::path &path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file)
        return {};
    const auto size = file.tellg();
    if (size <= 0)
        return {};
    std::string content(static_cast<size_t>(size), '\0');
    file.seekg(0);
    file.read(content.data(), size);
    return content;
}

id<MTLFunction> PipelineCache::compile_function(const std::vector<uint32_t> &spirv, bool is_vertex, const std::string &label, const fs::path &msl_cache_path) {
    @autoreleasepool {
        MetalDevice &dev = *state.device;

        std::string msl_source;
        std::string entry_point = is_vertex ? "main_vs" : "main_fs";
        if (!msl_cache_path.empty())
            msl_source = read_text_file(msl_cache_path);

        if (msl_source.empty()) {
            if (spirv.empty())
                return nil;

            const msl::TranslateResult result = msl::translate_gxm_shader(spirv, is_vertex, dev.support_framebuffer_fetch);
            if (!result.ok) {
                LOG_ERROR("Failed to translate shader {} to MSL: {}", label, result.error);
                return nil;
            }
            msl_source = result.source;
            entry_point = result.entry_point;

            if (!msl_cache_path.empty())
                fs_utils::dump_data(msl_cache_path, msl_source.data(), msl_source.size());
        }

        MTLCompileOptions *options = [[MTLCompileOptions alloc] init];
        options.languageVersion = MTLLanguageVersion2_3;
        if (@available(macOS 15.0, *)) {
            options.mathMode = MTLMathModeSafe;
        } else {
            options.fastMathEnabled = NO;
        }

        NSError *error = nil;
        id<MTLLibrary> library = [dev.device newLibraryWithSource:[NSString stringWithUTF8String:msl_source.c_str()] options:options error:&error];
        if (!library) {
            LOG_ERROR("Failed to compile MSL shader {}: {}", label, error ? error.localizedDescription.UTF8String : "unknown error");
            if (!state.shaders_log_path.empty()) {
                fs::create_directories(state.shaders_log_path);
                const fs::path dump_path = state.shaders_log_path / (label + ".failed.metal");
                fs_utils::dump_data(dump_path, msl_source.data(), msl_source.size());
            }
            return nil;
        }

        id<MTLFunction> function = [library newFunctionWithName:[NSString stringWithUTF8String:entry_point.c_str()]];
        if (!function && library.functionNames.count > 0)
            function = [library newFunctionWithName:library.functionNames[0]];
        if (!function)
            LOG_ERROR("Could not find the entry point of shader {}", label);
        return function;
    }
}

id<MTLFunction> PipelineCache::retrieve_shader(const SceGxmProgram *program, const Sha256Hash &hash, bool is_vertex, bool maskupdate, MemState &mem, const shader::Hints &hints) {
    ShaderEntry *entry;
    bool must_compile = false;
    {
        // look if it is in the cache
        std::unique_lock<std::mutex> lock(shaders_mutex);
        entry = &shaders[hash];
        if (entry->compiling.load(std::memory_order_acquire)) {
            // another thread is compiling the same exact shader at the same time
            // it's no use re-compiling it, so just wait for the other thread being done
            lock.unlock();
            while (entry->compiling.load(std::memory_order_acquire))
                std::this_thread::yield();
            lock.lock();
        }

        if (entry->function == nil) {
            // now mark the shader as compiling so that other threads accessing it won't try to compile it a second time
            entry->compiling.store(true, std::memory_order_release);
            must_compile = true;
        }
    }

    if (!must_compile)
        return entry->function;

    const bool listed = manifest_lists(state, shaders_mutex, hash);
    if (listed) {
        id<MTLFunction> function = precompile_shader(hash, false);
        if (function)
            return function;
    }

    const std::string hash_text = hex_string(hash);
    LOG_INFO("Generating metal shader {}", hash_text);
    const std::string shader_version = fmt::format("mtl{}", shader::CURRENT_VERSION);

    // the shaders are generated for the Vulkan target, then translated to MSL
    const shader::usse::SpirvCode source = load_spirv_shader(*program, state.features, true, hints, maskupdate, state.shaders_path, state.shaders_log_path, shader_version, listed);

    const fs::path msl_path = state.shaders_path.empty() ? fs::path() : state.shaders_path / fmt::format("{}-{}.msl", shader_version, hash_text);
    id<MTLFunction> function = compile_function(source, is_vertex, hash_text, msl_path);

    {
        std::lock_guard<std::mutex> guard(shaders_mutex);
        entry->function = function;
        entry->compiling.store(false, std::memory_order_release);

        if (function) {
            // Save shader cache hashes
            // vertex and fragment shaders are not linked together so no need to associate them
            Sha256Hash empty_hash{};
            if (is_vertex) {
                state.shaders_cache_hashs.push_back({ empty_hash, hash });
            } else {
                state.shaders_cache_hashs.push_back({ hash, empty_hash });
            }
        }
    }

    return function;
}

id<MTLFunction> PipelineCache::precompile_shader(const Sha256Hash &hash, bool search_first) {
    if (search_first) {
        std::lock_guard<std::mutex> guard(shaders_mutex);
        auto it = shaders.find(hash);
        if (it != shaders.end() && it->second.function)
            return it->second.function;
    }

    if (state.shaders_path.empty() || !fs::exists(state.shaders_path) || fs::is_empty(state.shaders_path))
        return nil;

    bool is_vertex = false;
    if (!manifest_lists(state, shaders_mutex, hash, &is_vertex))
        return nil;

    const std::string hash_text = hex_string(hash);
    const std::string shader_version = fmt::format("mtl{}", shader::CURRENT_VERSION);
    const fs::path spirv_path = state.shaders_path / fmt::format("{}-{}.spv", shader_version, hash_text);
    const fs::path msl_path = state.shaders_path / fmt::format("{}-{}.msl", shader_version, hash_text);

    std::vector<uint32_t> source;
    if (!fs::exists(msl_path))
        source = renderer::pre_load_shader_spirv(spirv_path);
    if (source.empty() && !fs::exists(msl_path))
        return nil;

    id<MTLFunction> function = compile_function(source, is_vertex, hash_text, msl_path);
    {
        std::lock_guard<std::mutex> guard(shaders_mutex);
        ShaderEntry &entry = shaders[hash];
        entry.function = function;
        entry.compiling.store(false, std::memory_order_release);
    }

    return function;
}

static uint32_t vertex_format_size(MTLVertexFormat format) {
    switch (format) {
    case MTLVertexFormatUChar:
    case MTLVertexFormatChar:
    case MTLVertexFormatUCharNormalized:
    case MTLVertexFormatCharNormalized:
        return 1;
    case MTLVertexFormatUChar2:
    case MTLVertexFormatChar2:
    case MTLVertexFormatUChar2Normalized:
    case MTLVertexFormatChar2Normalized:
    case MTLVertexFormatUShort:
    case MTLVertexFormatShort:
    case MTLVertexFormatUShortNormalized:
    case MTLVertexFormatShortNormalized:
    case MTLVertexFormatHalf:
        return 2;
    case MTLVertexFormatUChar3:
    case MTLVertexFormatChar3:
    case MTLVertexFormatUChar3Normalized:
    case MTLVertexFormatChar3Normalized:
        return 3;
    case MTLVertexFormatUChar4:
    case MTLVertexFormatChar4:
    case MTLVertexFormatUChar4Normalized:
    case MTLVertexFormatChar4Normalized:
    case MTLVertexFormatUShort2:
    case MTLVertexFormatShort2:
    case MTLVertexFormatUShort2Normalized:
    case MTLVertexFormatShort2Normalized:
    case MTLVertexFormatHalf2:
    case MTLVertexFormatFloat:
    case MTLVertexFormatInt:
    case MTLVertexFormatUInt:
        return 4;
    case MTLVertexFormatUShort3:
    case MTLVertexFormatShort3:
    case MTLVertexFormatUShort3Normalized:
    case MTLVertexFormatShort3Normalized:
    case MTLVertexFormatHalf3:
        return 6;
    case MTLVertexFormatUShort4:
    case MTLVertexFormatShort4:
    case MTLVertexFormatUShort4Normalized:
    case MTLVertexFormatShort4Normalized:
    case MTLVertexFormatHalf4:
    case MTLVertexFormatFloat2:
    case MTLVertexFormatInt2:
    case MTLVertexFormatUInt2:
        return 8;
    case MTLVertexFormatFloat3:
    case MTLVertexFormatInt3:
    case MTLVertexFormatUInt3:
        return 12;
    default:
        return 16;
    }
}

MTLVertexDescriptor *PipelineCache::get_vertex_descriptor(const SceGxmVertexProgram &vertex_program, MemState &mem) {
    MTLVertexDescriptor *descriptor = [MTLVertexDescriptor vertexDescriptor];

    // Vertex attributes.
    VertexProgram *vert = vertex_program.renderer_data.get();

    uint32_t used_streams = 0;
    uint32_t max_attribute_end[SCE_GXM_MAX_VERTEX_STREAMS] = {};

    for (const SceGxmVertexAttribute &attribute : vertex_program.attributes) {
        if (!vert->attribute_infos.contains(attribute.regIndex))
            continue;

        used_streams |= (1 << attribute.streamIndex);

        SceGxmAttributeFormat attribute_format = attribute.format;
        shader::usse::AttributeInformation info = vert->attribute_infos.at(attribute.regIndex);

        uint8_t component_count = attribute.componentCount;
        // these 2 values are only used when a matrix is used as a vertex attribute
        uint32_t array_size = 1;
        uint32_t array_element_size = 0;
        MTLVertexFormat format;
        if (info.regformat) {
            // use the data from the shader itself
            component_count = info.component_count;
            switch (info.gxm_type) {
            case SCE_GXM_PARAMETER_TYPE_U8:
            case SCE_GXM_PARAMETER_TYPE_S8:
            case SCE_GXM_PARAMETER_TYPE_C10:
                attribute_format = SCE_GXM_ATTRIBUTE_FORMAT_U8;
                break;
            case SCE_GXM_PARAMETER_TYPE_U16:
            case SCE_GXM_PARAMETER_TYPE_S16:
            case SCE_GXM_PARAMETER_TYPE_F16:
                attribute_format = SCE_GXM_ATTRIBUTE_FORMAT_U16;
                break;
            default:
                // U32 format
                attribute_format = SCE_GXM_ATTRIBUTE_FORMAT_UNTYPED;
                break;
            }

            if (info.gxm_type == SCE_GXM_PARAMETER_TYPE_C10)
                // this is 10-bit and not 8-bit
                component_count = (component_count * 10 + 7) / 8;

            if (component_count > 4) {
                // a matrix is used as an attribute, pack everything into an array of vec4
                array_size = (component_count + 3) / 4;
                array_element_size = 4 * gxm::attribute_format_size(attribute_format);
                component_count = 4;
            }

            // regformat attributes are int32
            format = translate_attribute_format(attribute_format, component_count, true, false);
        } else {
            // Metal does not support scaled attributes, the conversion is done in the shader
            info.is_integer = true;
            format = translate_attribute_format(attribute_format, component_count, info.is_integer, info.is_signed);
        }

        if (format == MTLVertexFormatInvalid)
            continue;

        for (uint32_t i = 0; i < array_size; i++) {
            MTLVertexAttributeDescriptor *attr = descriptor.attributes[info.location + i];
            attr.format = format;
            attr.bufferIndex = attribute.streamIndex;
            attr.offset = attribute.offset + i * array_element_size;
            max_attribute_end[attribute.streamIndex] = std::max<uint32_t>(max_attribute_end[attribute.streamIndex], attr.offset + vertex_format_size(format));
        }
    }

    for (unsigned int stream_index = 0; stream_index < SCE_GXM_MAX_VERTEX_STREAMS; stream_index++) {
        if (!(used_streams & (1 << stream_index)))
            continue;

        const SceGxmVertexStream &stream = vertex_program.streams[stream_index];
        const bool is_instanced = gxm::is_stream_instancing(static_cast<SceGxmIndexSource>(stream.indexSource));

        // Metal only allows strides which are multiple of 4, the streams are restrided when needed
        uint32_t stride = align(static_cast<uint32_t>(stream.stride), 4);
        if (stride == 0)
            stride = align(max_attribute_end[stream_index], 4);
        if (max_attribute_end[stream_index] > stride)
            LOG_WARN_ONCE("Vertex attribute of stream {} goes past the stream stride", stream_index);

        MTLVertexBufferLayoutDescriptor *layout = descriptor.layouts[stream_index];
        layout.stride = stride;
        layout.stepFunction = is_instanced ? MTLVertexStepFunctionPerInstance : MTLVertexStepFunctionPerVertex;
        layout.stepRate = 1;
    }

    return descriptor;
}

void PipelineCache::compiler_thread(MemState &mem) {
    moodycamel::ConsumerToken consumer_token(pipeline_compile_queue);

    // just a single loop, waiting for a pipeline compile request and compiling it
    CompileRequest *request;
    while (true) {
        pipeline_compile_queue.wait_dequeue(consumer_token, request);

        if (request == nullptr)
            // use this as an instruction to stop the thread
            break;

        id<MTLRenderPipelineState> pipeline = compile_pipeline(*request->vertex_program_gxm, *request->fragment_program_gxm, *request->get_record(), request->hints, request->color_format, request->has_depth, mem);
        request->entry->pipeline = pipeline;
        request->entry->ready.store(true, std::memory_order_release);

        request->vertex_program_gxm->compile_threads_on.fetch_sub(1, std::memory_order_release);
        request->fragment_program_gxm->compile_threads_on.fetch_sub(1, std::memory_order_release);

        const auto time_s = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        next_pipeline_cache_save = time_s + pipeline_cache_save_delay;

        state.shaders_count_compiled++;

        delete request;
    }
}

id<MTLRenderPipelineState> PipelineCache::compile_pipeline(const SceGxmVertexProgram &vertex_program_gxm, const SceGxmFragmentProgram &fragment_program_gxm, const GxmRecordState &record, const shader::Hints &hints, MTLPixelFormat color_format, bool has_depth, MemState &mem) {
    @autoreleasepool {
        MetalDevice &dev = *state.device;
        const VertexProgram &vertex_program = *vertex_program_gxm.renderer_data;
        const SceGxmProgram *gxm_fragment_shader = fragment_program_gxm.program.get(mem);
        const MetalFragmentProgram &fragment_program = *static_cast<MetalFragmentProgram *>(fragment_program_gxm.renderer_data.get());

        // the vertex input state must be computed before shader are retrieved in case symbols are stripped
        MTLVertexDescriptor *vertex_descriptor = get_vertex_descriptor(vertex_program_gxm, mem);

        const bool maskupdate = fragment_program_gxm.is_maskupdate;
        const bool is_fragment_disabled = !maskupdate && (record.front_side_fragment_program_mode == SCE_GXM_FRAGMENT_PROGRAM_DISABLED || gxm_fragment_shader->has_no_effect());

        id<MTLFunction> vertex_function = retrieve_shader(vertex_program_gxm.program.get(mem), vertex_program.hash, true, false, mem, hints);
        if (!vertex_function) {
            LOG_ERROR("Failed to retrieve the vertex shader of the pipeline");
            return nil;
        }

        id<MTLFunction> fragment_function = nil;
        if (is_fragment_disabled && state.features.use_mask_bit) {
            {
                std::lock_guard<std::mutex> lock(shaders_mutex);
                if (!mask_test_function) {
                    const auto code = shader::create_mask_test_shader();
                    mask_test_function = compile_function(code, false, "mask_test", {});
                }
                fragment_function = mask_test_function;
            }
        } else if (!is_fragment_disabled) {
            fragment_function = retrieve_shader(gxm_fragment_shader, fragment_program.hash, false, maskupdate, mem, hints);
            if (!fragment_function) {
                LOG_ERROR("Failed to retrieve the fragment shader of the pipeline");
                return nil;
            }
        }

        MTLRenderPipelineDescriptor *desc = [[MTLRenderPipelineDescriptor alloc] init];
        desc.vertexFunction = vertex_function;
        desc.fragmentFunction = fragment_function;
        desc.vertexDescriptor = vertex_descriptor;
        desc.rasterSampleCount = 1;
        desc.inputPrimitiveTopology = MTLPrimitiveTopologyClassUnspecified;

        MTLRenderPipelineColorAttachmentDescriptor *color_attachment = desc.colorAttachments[0];
        color_attachment.pixelFormat = maskupdate ? MTLPixelFormatRGBA8Unorm : color_format;

        const bool frag_has_no_output = static_cast<bool>(gxm_fragment_shader->program_flags & SCE_GXM_PROGRAM_FLAG_OUTPUT_UNDEFINED);
        if (maskupdate) {
            color_attachment.blendingEnabled = NO;
            color_attachment.writeMask = MTLColorWriteMaskAll;
        } else if (is_fragment_disabled || frag_has_no_output) {
            // The write mask must be empty as the lack of a fragment shader results in undefined values
            color_attachment.blendingEnabled = NO;
            color_attachment.writeMask = MTLColorWriteMaskNone;
        } else {
            const BlendState &blending = fragment_program.blending;
            color_attachment.blendingEnabled = blending.enable ? YES : NO;
            color_attachment.sourceRGBBlendFactor = static_cast<MTLBlendFactor>(blending.src_color);
            color_attachment.destinationRGBBlendFactor = static_cast<MTLBlendFactor>(blending.dst_color);
            color_attachment.rgbBlendOperation = static_cast<MTLBlendOperation>(blending.color_op);
            color_attachment.sourceAlphaBlendFactor = static_cast<MTLBlendFactor>(blending.src_alpha);
            color_attachment.destinationAlphaBlendFactor = static_cast<MTLBlendFactor>(blending.dst_alpha);
            color_attachment.alphaBlendOperation = static_cast<MTLBlendOperation>(blending.alpha_op);
            color_attachment.writeMask = static_cast<MTLColorWriteMask>(blending.write_mask);
        }

        if (has_depth) {
            desc.depthAttachmentPixelFormat = depth_stencil_format;
            desc.stencilAttachmentPixelFormat = depth_stencil_format;
        } else {
            desc.depthAttachmentPixelFormat = MTLPixelFormatInvalid;
            desc.stencilAttachmentPixelFormat = MTLPixelFormatInvalid;
        }

        NSError *error = nil;
        id<MTLRenderPipelineState> pipeline = [dev.device newRenderPipelineStateWithDescriptor:desc error:&error];
        if (!pipeline) {
            LOG_CRITICAL("Failed to create pipeline (vertex {}, fragment {}): {}", hex_string(vertex_program.hash), hex_string(fragment_program.hash),
                error ? error.localizedDescription.UTF8String : "unknown error");
            return nil;
        }

        return pipeline;
    }
}

id<MTLRenderPipelineState> PipelineCache::retrieve_pipeline(MetalContextImpl &context, bool consider_for_async, MemState &mem) {
    const GxmRecordState &record = context.owner.record;
    // get the hash of the current context
    uint64_t key = XXH3_64bits(&record, record_pipeline_len);

    // add the hash of the blending
    SceGxmFragmentProgram &fragment_program_gxm = *record.fragment_program.get(mem);
    const MetalFragmentProgram &fragment_program = *static_cast<MetalFragmentProgram *>(fragment_program_gxm.renderer_data.get());
    key ^= fragment_program.blending_hash;

    // add the hash of the attribute and stream layout
    SceGxmVertexProgram &vertex_program_gxm = *record.vertex_program.get(mem);
    key ^= vertex_program_gxm.key_hash;

    // the color format is not entirely determined by the record (packed formats may be unsupported)
    key ^= static_cast<uint64_t>(context.current_color_format) * 0x9E3779B97F4A7C15ULL;

    auto it = pipelines.find(key);
    if (it != pipelines.end()) {
        if (!it->second.ready.load(std::memory_order_acquire))
            // pipeline is still compiling
            return nil;
        return it->second.pipeline;
    }

    it = pipelines.emplace(std::piecewise_construct, std::forward_as_tuple(key), std::forward_as_tuple()).first;
    PipelineEntry &entry = it->second;

    const bool maskupdate = fragment_program_gxm.is_maskupdate;
    // update the shader hints
    context.owner.shader_hints.color_format = record.color_surface.colorFormat;
    context.owner.shader_hints.attributes = &vertex_program_gxm.attributes;

    const MTLPixelFormat color_format = maskupdate ? MTLPixelFormatRGBA8Unorm : context.current_color_format;
    const bool has_depth = !maskupdate;

    const bool compile_pipeline_async = !maskupdate && consider_for_async && use_async_compilation;

    if (compile_pipeline_async) {
        // create the pipeline compile request
        CompileRequest *request = new CompileRequest;
        request->entry = &entry;
        request->vertex_program_gxm = &vertex_program_gxm;
        request->fragment_program_gxm = &fragment_program_gxm;
        request->hints = context.owner.shader_hints;
        request->color_format = color_format;
        request->has_depth = has_depth;
        memcpy(request->record_data, &record, record_pipeline_len);

        // we must not delete these programs until the worker is done
        vertex_program_gxm.compile_threads_on.fetch_add(1, std::memory_order_relaxed);
        fragment_program_gxm.compile_threads_on.fetch_add(1, std::memory_order_relaxed);

        pipeline_compile_queue.enqueue(pipeline_compile_queue_token, request);

        return nil;
    } else {
        // can't wait, compile it right now
        id<MTLRenderPipelineState> result = compile_pipeline(vertex_program_gxm, fragment_program_gxm, record, context.owner.shader_hints, color_format, has_depth, mem);

        const auto time_s = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        next_pipeline_cache_save = time_s + pipeline_cache_save_delay;

        state.shaders_count_compiled++;

        entry.pipeline = result;
        entry.ready.store(true, std::memory_order_release);

        return result;
    }
}

static void fill_stencil_descriptor(MTLStencilDescriptor *descriptor, const GxmStencilStateOp &op, const GxmStencilStateValues &values) {
    descriptor.stencilCompareFunction = translate_stencil_func(op.func);
    descriptor.stencilFailureOperation = translate_stencil_op(op.stencil_fail);
    descriptor.depthFailureOperation = translate_stencil_op(op.depth_fail);
    descriptor.depthStencilPassOperation = translate_stencil_op(op.depth_pass);
    descriptor.readMask = values.compare_mask;
    descriptor.writeMask = values.write_mask;
}

id<MTLDepthStencilState> PipelineCache::get_disabled_depth_stencil_state() {
    if (!disabled_depth_stencil_state) {
        @autoreleasepool {
            MTLDepthStencilDescriptor *desc = [[MTLDepthStencilDescriptor alloc] init];
            desc.depthCompareFunction = MTLCompareFunctionAlways;
            desc.depthWriteEnabled = NO;
            disabled_depth_stencil_state = [state.device->device newDepthStencilStateWithDescriptor:desc];
        }
    }
    return disabled_depth_stencil_state;
}

id<MTLDepthStencilState> PipelineCache::retrieve_depth_stencil_state(const GxmRecordState &record, bool maskupdate) {
    if (maskupdate)
        return get_disabled_depth_stencil_state();

    const bool two_sided = (record.two_sided == SCE_GXM_TWO_SIDED_ENABLED);
    const GxmStencilStateOp &back_op = two_sided ? record.back_stencil_state_op : record.front_stencil_state_op;
    const GxmStencilStateValues &back_values = two_sided ? record.back_stencil_state_values : record.front_stencil_state_values;

    const DepthStencilKey key{
        .depth_func = static_cast<uint32_t>(record.front_depth_func),
        .depth_write = static_cast<uint32_t>(record.front_depth_write_mode == SCE_GXM_DEPTH_WRITE_ENABLED),
        .front_func = static_cast<uint32_t>(record.front_stencil_state_op.func),
        .front_fail = static_cast<uint32_t>(record.front_stencil_state_op.stencil_fail),
        .front_depth_fail = static_cast<uint32_t>(record.front_stencil_state_op.depth_fail),
        .front_pass = static_cast<uint32_t>(record.front_stencil_state_op.depth_pass),
        .front_compare_mask = record.front_stencil_state_values.compare_mask,
        .front_write_mask = record.front_stencil_state_values.write_mask,
        .back_func = static_cast<uint32_t>(back_op.func),
        .back_fail = static_cast<uint32_t>(back_op.stencil_fail),
        .back_depth_fail = static_cast<uint32_t>(back_op.depth_fail),
        .back_pass = static_cast<uint32_t>(back_op.depth_pass),
        .back_compare_mask = back_values.compare_mask,
        .back_write_mask = back_values.write_mask,
        .stencil_enabled = 1,
    };

    auto it = depth_stencil_states.find(key);
    if (it != depth_stencil_states.end())
        return it->second;

    @autoreleasepool {
        MTLDepthStencilDescriptor *desc = [[MTLDepthStencilDescriptor alloc] init];
        // depth and stencil tests are always enabled on the ps vita as there is almost no cost in doing so
        // on a tiled renderer
        desc.depthCompareFunction = translate_depth_func(record.front_depth_func);
        desc.depthWriteEnabled = key.depth_write ? YES : NO;
        fill_stencil_descriptor(desc.frontFaceStencil, record.front_stencil_state_op, record.front_stencil_state_values);
        fill_stencil_descriptor(desc.backFaceStencil, back_op, back_values);

        id<MTLDepthStencilState> ds_state = [state.device->device newDepthStencilStateWithDescriptor:desc];
        depth_stencil_states[key] = ds_state;
        return ds_state;
    }
}

id<MTLRenderPipelineState> PipelineCache::get_clear_pipeline(MTLPixelFormat color_format, bool has_color) {
    const uint32_t key = static_cast<uint32_t>(color_format) | (has_color ? 0x80000000u : 0u);
    auto it = clear_pipelines.find(key);
    if (it != clear_pipelines.end())
        return it->second;

    if (!clear_library)
        return nil;

    @autoreleasepool {
        MTLRenderPipelineDescriptor *desc = [[MTLRenderPipelineDescriptor alloc] init];
        desc.vertexFunction = [clear_library newFunctionWithName:@"clear_vs"];
        desc.fragmentFunction = [clear_library newFunctionWithName:@"clear_fs"];
        if (has_color) {
            desc.colorAttachments[0].pixelFormat = color_format;
            desc.colorAttachments[0].writeMask = MTLColorWriteMaskNone;
        }
        desc.depthAttachmentPixelFormat = depth_stencil_format;
        desc.stencilAttachmentPixelFormat = depth_stencil_format;

        NSError *error = nil;
        id<MTLRenderPipelineState> pipeline = [state.device->device newRenderPipelineStateWithDescriptor:desc error:&error];
        if (!pipeline)
            LOG_ERROR("Failed to create the clear pipeline: {}", error ? error.localizedDescription.UTF8String : "unknown error");
        clear_pipelines[key] = pipeline;
        return pipeline;
    }
}

id<MTLDepthStencilState> PipelineCache::get_clear_depth_stencil_state(bool clear_depth, bool clear_stencil) {
    const int index = (clear_depth ? 1 : 0) | (clear_stencil ? 2 : 0);
    if (clear_depth_stencil_states[index])
        return clear_depth_stencil_states[index];

    @autoreleasepool {
        MTLDepthStencilDescriptor *desc = [[MTLDepthStencilDescriptor alloc] init];
        desc.depthCompareFunction = MTLCompareFunctionAlways;
        desc.depthWriteEnabled = clear_depth ? YES : NO;
        for (MTLStencilDescriptor *stencil : { desc.frontFaceStencil, desc.backFaceStencil }) {
            stencil.stencilCompareFunction = MTLCompareFunctionAlways;
            stencil.stencilFailureOperation = MTLStencilOperationKeep;
            stencil.depthFailureOperation = MTLStencilOperationKeep;
            stencil.depthStencilPassOperation = clear_stencil ? MTLStencilOperationReplace : MTLStencilOperationKeep;
            stencil.readMask = 0xFF;
            stencil.writeMask = clear_stencil ? 0xFF : 0;
        }
        clear_depth_stencil_states[index] = [state.device->device newDepthStencilStateWithDescriptor:desc];
        return clear_depth_stencil_states[index];
    }
}

} // namespace renderer::metal
