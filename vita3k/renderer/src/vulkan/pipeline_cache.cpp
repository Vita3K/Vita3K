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

#include <renderer/vulkan/pipeline_cache.h>

#include <renderer/vulkan/gxm_to_vulkan.h>
#include <renderer/vulkan/state.h>
#include <renderer/vulkan/types.h>

#include <gxm/functions.h>
#include <gxm/types.h>
#include <renderer/shaders.h>
#include <shader/spirv_recompiler.h>

#include <util/fs.h>
#include <util/log.h>

#include <SDL3/SDL_cpuinfo.h>

// don't use the dispatch version, because we always hash a small amount
// with a known size
#define XXH_INLINE_ALL
#include <xxhash.h>

namespace renderer::vulkan {

// Size of the record containing what is needed for the pipeline construction (what is after is dynamic state)
constexpr size_t record_pipeline_len = offsetof(GxmRecordState, vertex_streams);

// structure containing everything needed to compile a pipeline
struct CompileRequest {
    // iterator to the pipeline location
    vk::Pipeline *pipeline;

    // this is everything we need to compile the shader on another thread (as the original data will change)
    SceGxmPrimitiveType type;
    vk::RenderPass render_pass;
    SceGxmVertexProgram *vertex_program_gxm;
    SceGxmFragmentProgram *fragment_program_gxm;
    shader::Hints hints;

    // the content of the record useful for the pipeline creation
    alignas(8) uint8_t record_data[record_pipeline_len];

    const GxmRecordState *get_record() {
        // note: this object is only half defined, but we are only looking at the part that's defined
        return reinterpret_cast<const GxmRecordState *>(record_data);
    }
};

PipelineCache::PipelineCache(VKState &state)
    : state(state)
    , pipeline_compile_queue_token(pipeline_compile_queue) {
}

void PipelineCache::init() {
    vk::PipelineCacheCreateInfo pipeline_info{};
    pipeline_cache = state.device.createPipelineCache(pipeline_info);

    // the layout for uniforms buffer can be made here as it will always be the same
    {
        std::array<vk::DescriptorSetLayoutBinding, 4> layout_bindings;
        // Our vertex uniform (GXMRenderVertUniformBlock)
        layout_bindings[0] = vk::DescriptorSetLayoutBinding{
            .binding = 0,
            .descriptorType = vk::DescriptorType::eUniformBufferDynamic,
            .descriptorCount = 1,
            .stageFlags = vk::ShaderStageFlagBits::eVertex,
        };
        // Our fragment uniform (GXMRenderFragUniformBlock)
        layout_bindings[1] = vk::DescriptorSetLayoutBinding{
            .binding = 1,
            .descriptorType = vk::DescriptorType::eUniformBufferDynamic,
            .descriptorCount = 1,
            .stageFlags = vk::ShaderStageFlagBits::eFragment,
        };
        // GXM vertex uniform (if no memory mapping)
        layout_bindings[2] = vk::DescriptorSetLayoutBinding{
            .binding = 2,
            .descriptorType = vk::DescriptorType::eStorageBufferDynamic,
            .descriptorCount = 1,
            .stageFlags = vk::ShaderStageFlagBits::eVertex,
        };
        // GXM Fragment uniform (if no memory mapping)
        layout_bindings[3] = vk::DescriptorSetLayoutBinding{
            .binding = 3,
            .descriptorType = vk::DescriptorType::eStorageBufferDynamic,
            .descriptorCount = 1,
            .stageFlags = vk::ShaderStageFlagBits::eFragment,
        };

        vk::DescriptorSetLayoutCreateInfo descriptor_info{
            .bindingCount = state.features.support_memory_mapping ? 2U : 4U,
            .pBindings = layout_bindings.data()
        };
        uniforms_layout = state.device.createDescriptorSetLayout(descriptor_info);
    }

    {
        // layout for the mask, color attachment as input, being an input attachment or a storage image
        // depending on whether or not we are using shader interlock
        std::array<vk::DescriptorSetLayoutBinding, 2> layout_binding;
        const vk::DescriptorType intput_image_descriptor = state.features.support_shader_interlock
            ? vk::DescriptorType::eStorageImage
            : vk::DescriptorType::eInputAttachment;
        layout_binding[0] = vk::DescriptorSetLayoutBinding{
            .binding = 0,
            .descriptorType = intput_image_descriptor,
            .descriptorCount = 1,
            .stageFlags = vk::ShaderStageFlagBits::eFragment
        };
        layout_binding[1] = vk::DescriptorSetLayoutBinding{
            .binding = 1,
            .descriptorType = vk::DescriptorType::eStorageImage,
            .descriptorCount = 1,
            .stageFlags = vk::ShaderStageFlagBits::eFragment
        };

        vk::DescriptorSetLayoutCreateInfo descriptor_info{
            .bindingCount = state.features.use_mask_bit ? 2U : 1U,
            .pBindings = layout_binding.data()
        };
        attachments_layout = state.device.createDescriptorSetLayout(descriptor_info);
    }

    {
        // texture layout

        // empty descriptor
        {
            vk::DescriptorSetLayoutCreateInfo empty_info{};
            vertex_textures_layout[0] = state.device.createDescriptorSetLayout(empty_info);
            fragment_textures_layout[0] = vertex_textures_layout[0];
        }

        // first vertex
        std::array<vk::DescriptorSetLayoutBinding, 16> layout_bindings;
        for (uint32_t i = 0; i < 16; i++) {
            layout_bindings[i] = {
                .binding = i,
                .descriptorType = vk::DescriptorType::eCombinedImageSampler,
                .descriptorCount = 1,
                .stageFlags = vk::ShaderStageFlagBits::eVertex
            };
        }
        for (uint32_t i = 1; i <= 16; i++) {
            vk::DescriptorSetLayoutCreateInfo descriptor_info{
                .bindingCount = i,
                .pBindings = layout_bindings.data()
            };
            vertex_textures_layout[i] = state.device.createDescriptorSetLayout(descriptor_info);
        }

        // then fragment
        for (uint32_t i = 0; i < 16; i++) {
            layout_bindings[i].stageFlags = vk::ShaderStageFlagBits::eFragment;
        }
        for (uint32_t i = 1; i <= 16; i++) {
            vk::DescriptorSetLayoutCreateInfo descriptor_info{
                .bindingCount = i,
                .pBindings = layout_bindings.data()
            };
            fragment_textures_layout[i] = state.device.createDescriptorSetLayout(descriptor_info);
        }
    }

    // compute all possible pipeline layouts
    for (uint32_t vert_texture_count = 0; vert_texture_count <= 16; vert_texture_count++) {
        for (uint32_t frag_texture_count = 0; frag_texture_count <= 16; frag_texture_count++) {
            vk::PipelineLayoutCreateInfo layout_info{};
            vk::DescriptorSetLayout set_layouts[] = { uniforms_layout, attachments_layout, vertex_textures_layout[vert_texture_count], fragment_textures_layout[frag_texture_count] };
            layout_info.setSetLayouts(set_layouts);
            pipeline_layouts[vert_texture_count][frag_texture_count] = state.device.createPipelineLayout(layout_info);
        }
    }

    {
        // look for rgb vertex attribute support
        // we need to look at each format because it is not the same for all usual 3-component formats (checked on AMD Radeon HD 7800)
        // no need to test for 32-bit types, they are always supported
        vk::Format formats[] = {
            vk::Format::eR16G16B16Unorm, vk::Format::eR16G16B16Snorm,
            vk::Format::eR16G16B16Uscaled, vk::Format::eR16G16B16Sscaled,
            vk::Format::eR16G16B16Uint, vk::Format::eR16G16B16Sint,
            vk::Format::eR16G16B16Sfloat,
            vk::Format::eR8G8B8Unorm, vk::Format::eR8G8B8Snorm,
            vk::Format::eR8G8B8Uscaled, vk::Format::eR8G8B8Sscaled,
            vk::Format::eR8G8B8Uint, vk::Format::eR8G8B8Sint
        };
        for (auto fmt : formats) {
            vk::FormatProperties rgb_property = state.physical_device.getFormatProperties(fmt);
            if (!(rgb_property.bufferFeatures & vk::FormatFeatureFlagBits::eVertexBuffer)) {
                unsupported_rgb_vertex_attribute_formats.emplace(fmt);
            }
        }
        state.features.support_rgb_attributes = unsupported_rgb_vertex_attribute_formats.empty();
    }

    const int nb_logical_threads = SDL_GetNumLogicalCPUCores();
    // took this from RPCS3 (slightly modified)
    if (nb_logical_threads > 12)
        nb_worker_threads = 6;
    else if (nb_logical_threads > 8)
        nb_worker_threads = 4;
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
        // launch all the threads
        for (int i = 0; i < nb_worker_threads; i++) {
            std::thread thread(&PipelineCache::compiler_thread, this, std::ref(*state.mem));
            thread.detach();
        }
    } else {
        LOG_INFO("Asynchronous pipeline compilation is now disabled");

        // we assume that by the time set_async_compilation is called again with enable=true, all previous worker threads have already exited
        for (int i = 0; i < nb_worker_threads; i++)
            // if a thread receives nullptr, it exits
            pipeline_compile_queue.enqueue(nullptr);
    }
}

// magic number put at the beginning of the pipeline cache file
constexpr uint32_t pipeline_cache_magic = 0xBEEF4321;

void PipelineCache::read_pipeline_cache() {
    const std::string pipeline_cache_name = fmt::format("pipeline-cache-vk{}.dat", shader::CURRENT_VERSION);
    const fs::path path = state.shaders_path / pipeline_cache_name;

    fs::ifstream pipeline_cache_file(path, std::ios::in | std::ios::binary);
    if (!pipeline_cache_file.is_open())
        return;

    LOG_INFO("Found pipeline cache, reading...");

    pipeline_cache_file.seekg(0, fs::ifstream::end);
    size_t pipeline_size = pipeline_cache_file.tellg();
    pipeline_cache_file.seekg(0);

    if (pipeline_size < sizeof(uint32_t) + sizeof(size_t))
        return;

    // read the hashes
    auto read_integer = [&]<typename T>(T &val) {
        pipeline_cache_file.read(reinterpret_cast<char *>(&val), sizeof(T));
    };
    uint32_t magic_number;
    read_integer(magic_number);
    size_t nb_hashes;
    read_integer(nb_hashes);
    // safety check
    size_t hashes_size = sizeof(magic_number) + sizeof(nb_hashes) + nb_hashes * sizeof(uint64_t);
    if (magic_number != pipeline_cache_magic || pipeline_size < hashes_size) {
        LOG_WARN("Pipeline cache is corrupted, ignoring it.");
        pipeline_cache_file.close();
        return;
    }
    pipeline_size -= hashes_size;

    // insert hashes with null pipeline
    for (size_t i = 0; i < nb_hashes; i++) {
        uint64_t hash;
        read_integer(hash);
        pipelines[hash] = nullptr;
    }

    std::vector<char> pipeline_data(pipeline_size);
    pipeline_cache_file.read(pipeline_data.data(), pipeline_size);
    pipeline_cache_file.close();

    vk::PipelineCacheCreateInfo cache_info{
        .initialDataSize = pipeline_size,
        .pInitialData = pipeline_data.data()
    };

    state.device.destroyPipelineCache(pipeline_cache);
    pipeline_cache = state.device.createPipelineCache(cache_info);
    LOG_INFO("Pipeline cache read and loaded");
}

void PipelineCache::save_pipeline_cache() {
    // first save the shader hashes
    // do a copy for thread safety
    std::vector<ShadersHash> shader_cache_copy;
    {
        std::lock_guard<std::mutex> guard(shaders_mutex);
        shader_cache_copy = state.shaders_cache_hashs;
    }
    renderer::save_shaders_cache_hashs(state, shader_cache_copy);

    const std::vector<uint8_t> pipeline_data = state.device.getPipelineCacheData(pipeline_cache);
    if (pipeline_data.empty())
        // No pipeline was created
        return;

    const std::string pipeline_cache_name = fmt::format("pipeline-cache-vk{}.dat", shader::CURRENT_VERSION);
    const fs::path path = state.shaders_path / pipeline_cache_name;

    fs::ofstream pipeline_cache_file(path, std::ios::out | std::ios::binary | std::ios::trunc);
    if (!pipeline_cache_file.is_open())
        return;

    LOG_INFO("Saving pipeline cache...");

    // first save the hashes of all pipelines
    auto write_integer = [&]<typename T>(T val) {
        pipeline_cache_file.write(reinterpret_cast<const char *>(&val), sizeof(T));
    };
    write_integer(pipeline_cache_magic);
    write_integer(pipelines.size());
    for (auto &[hash, _] : pipelines) {
        write_integer(hash);
    }

    // then save the cache
    pipeline_cache_file.write(reinterpret_cast<const char *>(pipeline_data.data()), pipeline_data.size());
    pipeline_cache_file.close();
    LOG_INFO("Pipeline cache saved");
}

// Vulkan structs used to specify a specialization constant
// Also, booleans in SPIRV are 32bit wide
static const vk::SpecializationMapEntry srgb_entry = {
    .constantID = shader::GAMMA_CORRECTION_SPECIALIZATION_ID,
    .offset = 0,
    .size = sizeof(uint32_t)
};

static const uint32_t srgb_entry_true = vk::True;
static const uint32_t srgb_entry_false = vk::False;

static const vk::SpecializationInfo srgb_info_true = {
    .mapEntryCount = 1,
    .pMapEntries = &srgb_entry,
    .dataSize = sizeof(uint32_t),
    .pData = &srgb_entry_true
};

static const vk::SpecializationInfo srgb_info_false = {
    .mapEntryCount = 1,
    .pMapEntries = &srgb_entry,
    .dataSize = sizeof(uint32_t),
    .pData = &srgb_entry_false
};

vk::PipelineShaderStageCreateInfo PipelineCache::retrieve_shader(const SceGxmProgram *program, const Sha256Hash &hash, bool is_vertex, bool maskupdate, MemState &mem, const shader::Hints &hints, bool is_srgb) {
    if (maskupdate)
        LOG_CRITICAL("Mask not implemented in the vulkan renderer!");

    const vk::ShaderModule shader_compiling = std::bit_cast<vk::ShaderModule>(~0ULL);

    const vk::SpecializationInfo *spec_info = nullptr;
    if (!is_vertex && state.features.should_use_shader_interlock() && program->is_frag_color_used()) {
        // if the specialization constant is used in the shader
        spec_info = is_srgb ? &srgb_info_true : &srgb_info_false;
    }

    vk::ShaderModule *shader_module;
    {
        // look if it is in the cache
        std::unique_lock<std::mutex> lock(shaders_mutex);
        shader_module = &shaders.insert({ hash, nullptr }).first->second;
        if (*shader_module == shader_compiling) {
            // another thread is compiling the same exact shader at the same time
            // it's no use re-compiling it, so just wait for the other thread being done
            lock.unlock();

            // we shouldn't need atomics and the compiler shouldn't be able to optimize this
            while (*shader_module == shader_compiling)
                std::this_thread::yield();
        }

        if (*shader_module == nullptr)
            // now mark the shader as compiling so that other threads accessing it won't try to compile it a second time
            *shader_module = shader_compiling;
    }

    if (*shader_module == shader_compiling) {
        precompile_shader(hash, false);
    }

    if (*shader_module != shader_compiling) {
        vk::PipelineShaderStageCreateInfo shader_stage_info{
            .stage = is_vertex ? vk::ShaderStageFlagBits::eVertex : vk::ShaderStageFlagBits::eFragment,
            .module = *shader_module,
            .pName = is_vertex ? "main_vs" : "main_fs",
            .pSpecializationInfo = spec_info,
        };
        return shader_stage_info;
    }

    const std::string hash_text = hex_string(hash);

    LOG_INFO("Generating vulkan spv shader {}", hash_text);
    const std::string shader_version = fmt::format("vk{}", shader::CURRENT_VERSION);

    shader::usse::SpirvCode source = load_spirv_shader(*program, state.features, true, hints, maskupdate, state.shaders_path, state.shaders_log_path, shader_version, true);

    vk::ShaderModuleCreateInfo shader_info{
        .codeSize = sizeof(uint32_t) * source.size(),
        .pCode = source.data()
    };

    *shader_module = state.device.createShaderModule(shader_info);
    {
        std::lock_guard<std::mutex> guard(shaders_mutex);
        // Save shader cache hashes
        // vertex and fragment shaders are not linked together so no need to associate them
        Sha256Hash empty_hash{};
        if (is_vertex) {
            state.shaders_cache_hashs.push_back({ hash, empty_hash });
        } else {
            state.shaders_cache_hashs.push_back({ empty_hash, hash });
        }
    }

    vk::PipelineShaderStageCreateInfo shader_stage_info{
        .stage = is_vertex ? vk::ShaderStageFlagBits::eVertex : vk::ShaderStageFlagBits::eFragment,
        .module = *shader_module,
        .pName = is_vertex ? "main_vs" : "main_fs",
        .pSpecializationInfo = spec_info,
    };

    return shader_stage_info;
}

vk::RenderPass PipelineCache::retrieve_render_pass(vk::Format format, bool force_load, bool force_store, bool no_color) {
    auto &render_passes_map = no_color ? shader_interlock_pass : render_passes[force_load][force_store];

    auto it = render_passes_map.find(format);

    if (it != render_passes_map.end())
        return it->second;

    // create a new render pass for this format

    vk::AttachmentReference color_ref{
        .attachment = 0,
        .layout = vk::ImageLayout::eGeneral
    };
    vk::AttachmentReference ds_ref{
        .attachment = no_color ? 0U : 1U,
        .layout = vk::ImageLayout::eDepthStencilAttachmentOptimal
    };
    vk::SubpassDescription subpass{
        .pipelineBindPoint = vk::PipelineBindPoint::eGraphics
    };
    subpass.setPDepthStencilAttachment(&ds_ref);
    if (!no_color) {
        subpass.setColorAttachments(color_ref);
        subpass.setInputAttachments(color_ref);
    }

    vk::AttachmentDescription color_attachment{
        .format = format,
        .samples = vk::SampleCountFlagBits::e1,
        .loadOp = vk::AttachmentLoadOp::eLoad,
        .storeOp = vk::AttachmentStoreOp::eStore,
        .initialLayout = vk::ImageLayout::eGeneral,
        .finalLayout = vk::ImageLayout::eGeneral
    };

    vk::AttachmentLoadOp load_op = force_load ? vk::AttachmentLoadOp::eLoad : vk::AttachmentLoadOp::eClear;
    vk::AttachmentStoreOp store_op = force_store ? vk::AttachmentStoreOp::eStore : vk::AttachmentStoreOp::eDontCare;
    vk::AttachmentDescription ds_attachment{
        .format = vk::Format::eD32SfloatS8Uint,
        .samples = vk::SampleCountFlagBits::e1,
        .loadOp = load_op,
        .storeOp = store_op,
        .stencilLoadOp = load_op,
        .stencilStoreOp = store_op,
        .initialLayout = force_load ? vk::ImageLayout::eDepthStencilReadOnlyOptimal : vk::ImageLayout::eUndefined,
        .finalLayout = vk::ImageLayout::eDepthStencilReadOnlyOptimal
    };

    std::array<vk::SubpassDependency, 4> dependencies;

    // external dependency
    // we want the previous render pass to be done when we reach the fragment stage / stencil*depth testing
    dependencies[0] = {
        .srcSubpass = VK_SUBPASS_EXTERNAL,
        .dstSubpass = 0,
        .srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput | vk::PipelineStageFlagBits::eLateFragmentTests,
        .dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput | vk::PipelineStageFlagBits::eEarlyFragmentTests,
        .srcAccessMask = vk::AccessFlagBits::eColorAttachmentWrite | vk::AccessFlagBits::eDepthStencilAttachmentWrite,
        .dstAccessMask = vk::AccessFlagBits::eColorAttachmentRead | vk::AccessFlagBits::eDepthStencilAttachmentRead
    };

    if (state.features.support_shader_interlock && no_color) {
        // we must wait for the previous shaders to be done
        dependencies[1].dstStageMask = vk::PipelineStageFlagBits::eFragmentShader;
        dependencies[1].dstAccessMask = vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite;
    }

    // if an attachment is sampled from, we want it to be done before the next render pass fragment shader
    dependencies[1] = {
        .srcSubpass = VK_SUBPASS_EXTERNAL,
        .dstSubpass = 0,
        .srcStageMask = vk::PipelineStageFlagBits::eFragmentShader,
        .dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput | vk::PipelineStageFlagBits::eLateFragmentTests,
        .srcAccessMask = vk::AccessFlagBits::eShaderRead,
        .dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite | vk::AccessFlagBits::eDepthStencilAttachmentWrite
    };

    if (state.features.support_shader_interlock && !no_color) {
        // we must wait for the shader interlock shader to be done
        dependencies[1].srcAccessMask |= vk::AccessFlagBits::eShaderWrite;
    }

    // self-dependency
    // this allows us to use a pipeline barrier in the render pass for programmable blending
    dependencies[2] = {
        .srcSubpass = 0,
        .dstSubpass = 0,
        .srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput,
        .dstStageMask = vk::PipelineStageFlagBits::eFragmentShader,
        .srcAccessMask = vk::AccessFlagBits::eColorAttachmentWrite,
        .dstAccessMask = vk::AccessFlagBits::eInputAttachmentRead,
        .dependencyFlags = vk::DependencyFlagBits::eByRegion
    };

    // mid-scene flush
    // unity games use it to write to a buffer in a vertex shader then use it as the vertex input in the next draw
    dependencies[3] = {
        .srcSubpass = 0,
        .dstSubpass = 0,
        .srcStageMask = vk::PipelineStageFlagBits::eVertexShader,
        .dstStageMask = vk::PipelineStageFlagBits::eVertexInput,
        .srcAccessMask = vk::AccessFlagBits::eShaderWrite,
        .dstAccessMask = vk::AccessFlagBits::eVertexAttributeRead
    };

    vk::RenderPassCreateInfo pass_info{};
    vk::AttachmentDescription attachments[] = { color_attachment, ds_attachment };
    pass_info.setAttachments(attachments);
    pass_info.setSubpasses(subpass);
    pass_info.setDependencies(dependencies);
    if (no_color) {
        // only add the ds attachment
        pass_info.pAttachments = &attachments[1];
        pass_info.attachmentCount = 1;
        // no need for the self-dependency
        pass_info.setDependencyCount(2);
    }

    render_passes_map[format] = state.device.createRenderPass(pass_info);

    return render_passes_map[format];
}

vk::PipelineVertexInputStateCreateInfo PipelineCache::get_vertex_input_state(const SceGxmVertexProgram &vertex_program, MemState &mem) {
    // pointer to these objects are returned (so it needs to be static)
    // and each thread needs one (hence the thread_local)
    static thread_local std::vector<vk::VertexInputBindingDescription> binding_descr;
    static thread_local std::vector<vk::VertexInputAttributeDescription> attr_descr;
    binding_descr.clear();
    attr_descr.clear();

    // Vertex attributes.
    VertexProgram *vkvert = vertex_program.renderer_data.get();

    uint32_t used_streams = 0;

    for (const SceGxmVertexAttribute &attribute : vertex_program.attributes) {
        if (!vkvert->attribute_infos.contains(attribute.regIndex))
            continue;

        used_streams |= (1 << attribute.streamIndex);

        SceGxmAttributeFormat attribute_format = attribute.format;
        shader::usse::AttributeInformation info = vkvert->attribute_infos.at(attribute.regIndex);

        uint8_t component_count = attribute.componentCount;
        // these 2 values are only used when a matrix is used as a vertex attribute
        // this is only supported for regformated attribute for now
        // TODO: add support for matrix input for non-regformated attributes
        uint32_t array_size = 1;
        uint32_t array_element_size = 0;
        vk::Format format;
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
            if (component_count == 3 && unsupported_rgb_vertex_attribute_formats.contains(format)) {
                component_count = 4;
                format = translate_attribute_format(attribute_format, component_count, true, false);
            }
        } else {
            // some AMD GPUs do not support rgb vertex attributes, so just put it as rgba
            // the 4th component will contain garbage but this is not an issue because the input
            // in the shader will be vec3 (or ivec3) and the 4th component will be discarded
            format = translate_attribute_format(attribute_format, component_count, info.is_integer, info.is_signed);
            if (component_count == 3 && unsupported_rgb_vertex_attribute_formats.contains(format)) {
                component_count = 4;
                format = translate_attribute_format(attribute_format, component_count, info.is_integer, info.is_signed);
            }
        }

        for (uint32_t i = 0; i < array_size; i++) {
            attr_descr.push_back(vk::VertexInputAttributeDescription{
                .location = info.location + i,
                .binding = attribute.streamIndex,
                .format = format,
                .offset = attribute.offset + i * array_element_size });
        }
    }

    for (unsigned int stream_index = 0; stream_index < SCE_GXM_MAX_VERTEX_STREAMS; stream_index++) {
        if (!(used_streams & (1 << stream_index)))
            continue;

        const SceGxmVertexStream &stream = vertex_program.streams[stream_index];

        const bool is_instanced = gxm::is_stream_instancing(static_cast<SceGxmIndexSource>(stream.indexSource));

#ifdef __APPLE__
        const uint32_t stride = align(stream.stride, 4);
#else
        const uint32_t stride = stream.stride;
#endif
        binding_descr.push_back(vk::VertexInputBindingDescription{
            .binding = stream_index,
            .stride = stride,
            .inputRate = is_instanced ? vk::VertexInputRate::eInstance : vk::VertexInputRate::eVertex });
    }

    vk::PipelineVertexInputStateCreateInfo vertex_input{};
    vertex_input.setVertexBindingDescriptions(binding_descr);
    vertex_input.setVertexAttributeDescriptions(attr_descr);
    return vertex_input;
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

        vk::Pipeline pipeline = compile_pipeline(request->type, request->render_pass, *request->vertex_program_gxm, *request->fragment_program_gxm, *request->get_record(), request->hints, mem);
        *request->pipeline = pipeline;

        request->vertex_program_gxm->compile_threads_on.fetch_sub(1, std::memory_order_release);
        request->fragment_program_gxm->compile_threads_on.fetch_sub(1, std::memory_order_release);

        const auto time_s = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        next_pipeline_cache_save = time_s + pipeline_cache_save_delay;

        state.shaders_count_compiled++;

        delete request;
    }
}

static vk::StencilOpState convert_op_state(const GxmStencilStateOp &state) {
    return vk::StencilOpState{
        .failOp = translate_stencil_op(state.stencil_fail),
        .passOp = translate_stencil_op(state.depth_pass),
        .depthFailOp = translate_stencil_op(state.depth_fail),
        .compareOp = translate_stencil_func(state.func)
    };
}

vk::Pipeline PipelineCache::compile_pipeline(SceGxmPrimitiveType type, vk::RenderPass render_pass, const SceGxmVertexProgram &vertex_program_gxm, const SceGxmFragmentProgram &fragment_program_gxm, const GxmRecordState &record, const shader::Hints &hints, MemState &mem) {
    const VertexProgram &vertex_program = *vertex_program_gxm.renderer_data;
    const SceGxmProgram *gxm_fragment_shader = fragment_program_gxm.program.get(mem);
    const VKFragmentProgram &fragment_program = *reinterpret_cast<VKFragmentProgram *>(
        fragment_program_gxm.renderer_data.get());

    // the vertex input state must be computed before shader are retrieved in case symbols are stripped
    const vk::PipelineVertexInputStateCreateInfo vertex_input = get_vertex_input_state(vertex_program_gxm, mem);

    const vk::PipelineShaderStageCreateInfo vertex_shader = retrieve_shader(vertex_program_gxm.program.get(mem), vertex_program.hash, true, fragment_program_gxm.is_maskupdate, mem, hints);
    const vk::PipelineShaderStageCreateInfo fragment_shader = retrieve_shader(gxm_fragment_shader, fragment_program.hash, false, fragment_program_gxm.is_maskupdate, mem, hints, record.is_gamma_corrected);
    const vk::PipelineShaderStageCreateInfo shader_stages[] = { vertex_shader, fragment_shader };
    // disable the fragment shader if gxm asks us to
    const bool is_fragment_disabled = record.front_side_fragment_program_mode == SCE_GXM_FRAGMENT_PROGRAM_DISABLED || gxm_fragment_shader->has_no_effect();
    const uint32_t shader_stage_count = is_fragment_disabled ? 1U : 2U;

    const vk::PipelineInputAssemblyStateCreateInfo input_assembly{
        .topology = translate_primitive(type)
    };

    const bool two_sided = (record.two_sided == SCE_GXM_TWO_SIDED_ENABLED);

    const bool use_shader_interlock = state.features.support_shader_interlock && gxm_fragment_shader->is_frag_color_used();

    const vk::PipelineRasterizationStateCreateInfo rasterizer{
        .depthClampEnable = state.physical_device_features.depthClamp,
        .polygonMode = translate_polygon_mode(record.front_polygon_mode),
        .cullMode = translate_cull_mode(record.cull_mode),
        // front face is always counter clockwise
        .frontFace = vk::FrontFace::eCounterClockwise,
        .depthBiasEnable = VK_TRUE
    };
    const vk::PipelineMultisampleStateCreateInfo multisampling{
        .rasterizationSamples = vk::SampleCountFlagBits::e1
    };
    // depth and stencil tests are always enabled on the ps vita as there is almost no cost in doing so
    // on a tiled renderer
    const vk::PipelineDepthStencilStateCreateInfo ds_info{
        .depthTestEnable = VK_TRUE,
        .depthWriteEnable = (record.front_depth_write_mode == SCE_GXM_DEPTH_WRITE_ENABLED),
        .depthCompareOp = translate_depth_func(record.front_depth_func),
        .depthBoundsTestEnable = VK_FALSE,
        .stencilTestEnable = VK_TRUE,
        .front = convert_op_state(record.front_stencil_state_op),
        .back = convert_op_state(two_sided ? record.back_stencil_state_op : record.front_stencil_state_op)
    };

    vk::PipelineColorBlendStateCreateInfo color_blending{};
    const bool frag_has_no_output = static_cast<bool>(gxm_fragment_shader->program_flags & SCE_GXM_PROGRAM_FLAG_OUTPUT_UNDEFINED);
    if (is_fragment_disabled || frag_has_no_output || use_shader_interlock) {
        // The write mask must be empty as the lack of a fragment shader results in undefined values
        static const vk::PipelineColorBlendAttachmentState blending = {
            .blendEnable = VK_FALSE,
            .colorWriteMask = vk::ColorComponentFlags()
        };
        color_blending.setAttachments(blending);
    } else {
        const vk::PipelineColorBlendAttachmentState &blending = fragment_program.blending;
        color_blending.setAttachments(blending);
    }

    vk::PipelineLayout pipeline_layout = pipeline_layouts[vertex_program.texture_count][fragment_program.texture_count];

    // all of these can be changed at any time using the vita graphics api (like opengl)
    // Because each one can take a lot of different values, it's better to set them as dynamic
    static vk::DynamicState dynamic_states[] = {
        vk::DynamicState::eViewport,
        vk::DynamicState::eScissor,
        vk::DynamicState::eLineWidth,
        vk::DynamicState::eStencilCompareMask,
        vk::DynamicState::eStencilReference,
        vk::DynamicState::eStencilWriteMask,
        vk::DynamicState::eDepthBias
    };
    vk::PipelineDynamicStateCreateInfo dynamic_info{};
    dynamic_info.setDynamicStates(dynamic_states);

    // we still need to specify the viewport and scissor count even though they are dynamic
    vk::PipelineViewportStateCreateInfo viewport{
        .viewportCount = 1,
        .scissorCount = 1
    };

    vk::GraphicsPipelineCreateInfo pipeline_info{
        .stageCount = shader_stage_count,
        .pStages = shader_stages,
        .pVertexInputState = &vertex_input,
        .pInputAssemblyState = &input_assembly,
        .pViewportState = &viewport,
        .pRasterizationState = &rasterizer,
        .pMultisampleState = &multisampling,
        .pDepthStencilState = &ds_info,
        .pColorBlendState = &color_blending,
        .pDynamicState = &dynamic_info,
        .layout = pipeline_layout,
        .renderPass = render_pass,
        .subpass = 0
    };

    const auto result = state.device.createGraphicsPipeline(pipeline_cache, pipeline_info);
    if (result.result != vk::Result::eSuccess) {
        LOG_CRITICAL("Failed to create pipeline.");
        return nullptr;
    }

    return result.value;
}

vk::Pipeline PipelineCache::retrieve_pipeline(VKContext &context, SceGxmPrimitiveType &type, bool consider_for_async, MemState &mem) {
    const GxmRecordState &record = context.record;
    // get the hash of the current context
    uint64_t key = XXH3_64bits(&record, record_pipeline_len);

    // add the hash of the blending
    SceGxmFragmentProgram &fragment_program_gxm = *record.fragment_program.get(mem);
    const VKFragmentProgram &fragment_program = *reinterpret_cast<VKFragmentProgram *>(
        fragment_program_gxm.renderer_data.get());
    key ^= fragment_program.blending_hash;

    // add the hash of the attribute and stream layout
    SceGxmVertexProgram &vertex_program_gxm = *record.vertex_program.get(mem);
    key ^= vertex_program_gxm.key_hash;

    // and also add the primitive type
    key ^= static_cast<uint64_t>(type);

    // can't use constexpr because of apple clang...
    const vk::Pipeline pipeline_compiling = std::bit_cast<vk::Pipeline, uint64_t>(~0ULL);
    // if the pipeline is in the pipeline cache, we can expect its creation time to be almost instantaneous
    bool already_in_cache = false;

    auto it = pipelines.find(key);
    if (it != pipelines.end()) {
        if (it->second != nullptr) {
            if (it->second == pipeline_compiling)
                // pipeline is still compiling
                return nullptr;
            else
                return it->second;
        }
        already_in_cache = true;
    } else {
        // the pipeline hash was not in the cache;
        it = pipelines.insert({ key, pipeline_compiling }).first;
    }

    // get the correct renderpass here
    const SceGxmProgram *gxm_fragment_shader = fragment_program_gxm.program.get(mem);
    const bool use_shader_interlock = state.features.support_shader_interlock && gxm_fragment_shader->is_frag_color_used();
    const vk::RenderPass render_pass = use_shader_interlock ? context.current_shader_interlock_pass : context.current_render_pass;
    // update the shader hints
    context.shader_hints.color_format = record.color_surface.colorFormat;
    context.shader_hints.attributes = &vertex_program_gxm.attributes;

    // note: the flag can_use_deferred_compilation is not considered here because it causes way too many false positives
    const bool compile_pipeline_async = !already_in_cache && consider_for_async && use_async_compilation;

    if (compile_pipeline_async) {
        // create the pipeline compile request
        CompileRequest *request = new CompileRequest;
        *request = {
            .pipeline = &it->second,
            .type = type,
            .render_pass = render_pass,
            .vertex_program_gxm = &vertex_program_gxm,
            .fragment_program_gxm = &fragment_program_gxm,
            .hints = context.shader_hints
        };
        memcpy(request->record_data, &record, record_pipeline_len);
        it->second = pipeline_compiling;

        // we must not delete these programs until the worker is done
        vertex_program_gxm.compile_threads_on.fetch_add(1, std::memory_order_relaxed);
        fragment_program_gxm.compile_threads_on.fetch_add(1, std::memory_order_relaxed);

        pipeline_compile_queue.enqueue(pipeline_compile_queue_token, request);

        return nullptr;
    } else {
        // can't wait, compile it right now
        vk::Pipeline result = compile_pipeline(type, render_pass, vertex_program_gxm, fragment_program_gxm, record, context.shader_hints, mem);

        const auto time_s = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        next_pipeline_cache_save = time_s + pipeline_cache_save_delay;

        if (!already_in_cache)
            state.shaders_count_compiled++;

        it->second = result;

        return result;
    }
}

vk::ShaderModule PipelineCache::precompile_shader(const Sha256Hash &hash, bool search_first) {
    if (search_first) {
        // happens while loading the thread, no parallel access so no need for a mutex
        auto it = shaders.find(hash);
        if (it != shaders.end())
            return it->second;
    }

    if (!fs::exists(state.shaders_path) || fs::is_empty(state.shaders_path))
        return nullptr;

    Sha256Hash shader_hash;
    memcpy(shader_hash.data(), hash.data(), sizeof(Sha256Hash));
    const std::string shader_file_name = fmt::format("vk{}-{}.spv", shader::CURRENT_VERSION, hex_string(shader_hash));
    const std::vector<uint32_t> source = renderer::pre_load_shader_spirv(state.shaders_path / shader_file_name);

    if (source.empty())
        return nullptr;

    vk::ShaderModuleCreateInfo shader_info{
        .codeSize = sizeof(uint32_t) * source.size(),
        .pCode = source.data()
    };

    vk::ShaderModule shader = state.device.createShaderModule(shader_info);
    {
        std::lock_guard<std::mutex> guard(shaders_mutex);
        shaders[hash] = shader;
    }

    return shader;
}
} // namespace renderer::vulkan
