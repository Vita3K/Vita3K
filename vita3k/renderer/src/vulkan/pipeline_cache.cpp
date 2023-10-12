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

#include <renderer/vulkan/pipeline_cache.h>

#include <xxh3.h>

#include <renderer/vulkan/gxm_to_vulkan.h>
#include <renderer/vulkan/state.h>
#include <renderer/vulkan/types.h>

#include <gxm/functions.h>
#include <gxm/types.h>
#include <renderer/shaders.h>
#include <shader/spirv_recompiler.h>

#include <util/align.h>
#include <util/fs.h>
#include <util/log.h>

namespace renderer::vulkan {
PipelineCache::PipelineCache(VKState &state)
    : state(state) {
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
        for (uint32_t i = 0; i < 17; i++) {
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
        for (uint32_t i = 0; i < 17; i++) {
            vk::DescriptorSetLayoutCreateInfo descriptor_info{
                .bindingCount = i,
                .pBindings = layout_bindings.data()
            };
            fragment_textures_layout[i] = state.device.createDescriptorSetLayout(descriptor_info);
        }
    }

    {
        // look for rgb vertex attribute support
        // we need to look at each format because it is not the same for all usual 3-component formats (checked on AMD Radeon HD 7800)
        std::array<vk::Format, 7> formats = { vk::Format::eR16G16B16Unorm, vk::Format::eR16G16B16Snorm, vk::Format::eR16G16B16Uscaled, vk::Format::eR16G16B16Sscaled,
            vk::Format::eR16G16B16Uint, vk::Format::eR16G16B16Sint, vk::Format::eR16G16B16Sfloat };
        for (auto fmt : formats) {
            vk::FormatProperties rgb_property = state.physical_device.getFormatProperties(fmt);
            if (!(rgb_property.bufferFeatures & vk::FormatFeatureFlagBits::eVertexBuffer)) {
                unsupported_rgb_vertex_attribute_formats.emplace(fmt);
            }
        }
        state.features.support_rgb_attributes = unsupported_rgb_vertex_attribute_formats.empty();
    }
}

void PipelineCache::read_pipeline_cache() {
    const auto shaders_path{ fs::path(state.base_path) / "cache/shaders" / state.title_id / state.self_name };
    const std::string pipeline_cache_name = fmt::format("pipeline-cache-vk{}.dat", shader::CURRENT_VERSION);
    const fs::path path = shaders_path / pipeline_cache_name;

    fs::ifstream pipeline_cache_file(path, std::ios::in | std::ios::binary);
    if (!pipeline_cache_file.is_open())
        return;

    LOG_INFO("Found pipeline cache, reading...");

    pipeline_cache_file.seekg(0, fs::ifstream::end);
    const size_t pipeline_size = pipeline_cache_file.tellg();
    pipeline_cache_file.seekg(0);

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
    const std::vector<uint8_t> pipeline_data = state.device.getPipelineCacheData(pipeline_cache);
    if (pipeline_data.empty())
        // No pipeline was created
        return;

    const auto shaders_path{ fs::path(state.base_path) / "cache/shaders" / state.title_id / state.self_name };
    const std::string pipeline_cache_name = fmt::format("pipeline-cache-vk{}.dat", shader::CURRENT_VERSION);
    const fs::path path = shaders_path / pipeline_cache_name;

    fs::ofstream pipeline_cache_file(path, std::ios::out | std::ios::binary);
    if (!pipeline_cache_file.is_open())
        return;

    LOG_INFO("Saving pipeline cache...");

    pipeline_cache_file.write(reinterpret_cast<const char *>(pipeline_data.data()), pipeline_data.size());
    pipeline_cache_file.close();
    LOG_INFO("Pipeline cache saved");
}

vk::PipelineShaderStageCreateInfo PipelineCache::retrieve_shader(const SceGxmProgram *program, const Sha256Hash &hash, bool is_vertex, bool maskupdate, MemState &mem, const std::vector<SceGxmVertexAttribute> *hint_attributes) {
    if (maskupdate)
        LOG_CRITICAL("Mask not implemented in the vulkan renderer!");

    auto it = shaders.find(hash);

    if (it == shaders.end()) {
        // look if it is in the cache
        if (precompile_shader(hash))
            it = shaders.find(hash);
    }

    if (it != shaders.end()) {
        vk::PipelineShaderStageCreateInfo shader_stage_info{
            .stage = is_vertex ? vk::ShaderStageFlagBits::eVertex : vk::ShaderStageFlagBits::eFragment,
            .module = it->second,
            .pName = is_vertex ? "main_vs" : "main_fs"
        };
        return shader_stage_info;
    }

    const char *base_path = state.base_path;
    const char *title_id = state.title_id;
    const char *self_name = state.self_name;

    const std::string hash_text = hex_string(hash);

    LOG_INFO("Generating vulkan spv shader {}", hash_text.data());
    const std::string shader_version = fmt::format("vk{}", shader::CURRENT_VERSION);

    // update shader hints
    current_context->shader_hints.color_format = current_context->record.color_surface.colorFormat;
    current_context->shader_hints.attributes = hint_attributes;

    shader::usse::SpirvCode source = load_spirv_shader(*program, state.features, true, current_context->shader_hints, maskupdate, base_path, title_id, self_name, shader_version, true);

    vk::ShaderModuleCreateInfo shader_info{
        .codeSize = sizeof(uint32_t) * source.size(),
        .pCode = source.data()
    };

    vk::ShaderModule shader = current_context->state.device.createShaderModule(shader_info);
    shaders[hash] = shader;

    // Save shader cache haches
    // vertex and fragment shaders are not linked together so no need to associate them
    Sha256Hash empty_hash{};
    if (is_vertex) {
        state.shaders_cache_hashs.push_back({ hash, empty_hash });
    } else {
        state.shaders_cache_hashs.push_back({ empty_hash, hash });
    }
    renderer::save_shaders_cache_hashs(state, state.shaders_cache_hashs);

    const auto time_s = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    next_pipeline_cache_save = time_s + pipeline_cache_save_delay;

    vk::PipelineShaderStageCreateInfo shader_stage_info{
        .stage = is_vertex ? vk::ShaderStageFlagBits::eVertex : vk::ShaderStageFlagBits::eFragment,
        .module = shader,
        .pName = is_vertex ? "main_vs" : "main_fs"
    };

    state.shaders_count_compiled++;

    return shader_stage_info;
}

vk::PipelineLayout PipelineCache::retrieve_pipeline_layout(const uint16_t vert_texture_count, const uint16_t frag_texture_count) {
    if (!pipeline_layouts[vert_texture_count][frag_texture_count]) {
        // create matching pipeline layout
        vk::PipelineLayoutCreateInfo layout_info{};
        vk::DescriptorSetLayout set_layouts[] = { uniforms_layout, attachments_layout, vertex_textures_layout[vert_texture_count], fragment_textures_layout[frag_texture_count] };
        layout_info.setSetLayouts(set_layouts);
        pipeline_layouts[vert_texture_count][frag_texture_count] = state.device.createPipelineLayout(layout_info);
    }

    return pipeline_layouts[vert_texture_count][frag_texture_count];
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

vk::PipelineVertexInputStateCreateInfo PipelineCache::get_vertex_input_state(MemState &mem) {
    // Vertex attributes.
    const GxmRecordState &state = current_context->record;
    const SceGxmVertexProgram &vertex_program = *state.vertex_program.get(mem);
    VertexProgram *vkvert = vertex_program.renderer_data.get();

    if (!vkvert->stripped_symbols_checked) {
        // Insert some symbols here
        const SceGxmProgram *vertex_program_body = vertex_program.program.get(mem);
        if (vertex_program_body && (vertex_program_body->primary_reg_count != 0)) {
            for (std::size_t i = 0; i < vertex_program.attributes.size(); i++) {
                vkvert->attribute_infos.emplace(vertex_program.attributes[i].regIndex, shader::usse::AttributeInformation(static_cast<std::uint16_t>(i), SCE_GXM_PARAMETER_TYPE_F32, false, false, false));
            }
        }

        vkvert->stripped_symbols_checked = true;
    }

    binding_descr.clear();
    attr_descr.clear();

    uint32_t used_streams = 0;

    for (const SceGxmVertexAttribute &attribute : vertex_program.attributes) {
        if (!vkvert->attribute_infos.contains(attribute.regIndex))
            continue;

        used_streams |= (1 << attribute.streamIndex);

        const SceGxmAttributeFormat attribute_format = static_cast<SceGxmAttributeFormat>(attribute.format);
        shader::usse::AttributeInformation info = vkvert->attribute_infos.at(attribute.regIndex);

        uint8_t component_count = attribute.componentCount;
        // these 2 values are only used when a matrix is used as a vertex attribute
        // this is only supported for regformated attribute for now
        // TODO: add support for matrix input for non-regformated attributes
        uint32_t array_size = 1;
        uint32_t array_element_size = 0;
        vk::Format format;
        if (info.regformat) {
            const int comp_size = gxm::attribute_format_size(attribute_format);
            component_count = (comp_size * component_count + 3) / 4;

            if (component_count > 4) {
                // a matrix is used as an attribute, pack everything into an array of vec4
                array_size = (component_count + 3) / 4;
                array_element_size = 4 * sizeof(int32_t);
                component_count = 4;
            }

            // regformat attributes are int32
            format = translate_attribute_format(SCE_GXM_ATTRIBUTE_FORMAT_UNTYPED, component_count, true, true);
            if (component_count == 3 && unsupported_rgb_vertex_attribute_formats.contains(format)) {
                component_count = 4;
                format = translate_attribute_format(SCE_GXM_ATTRIBUTE_FORMAT_UNTYPED, component_count, true, true);
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
                .location = info.location() + i,
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

static vk::StencilOpState convert_op_state(const GxmStencilStateOp &state) {
    return vk::StencilOpState{
        .failOp = translate_stencil_op(state.stencil_fail),
        .passOp = translate_stencil_op(state.depth_pass),
        .depthFailOp = translate_stencil_op(state.depth_fail),
        .compareOp = translate_stencil_func(state.func)
    };
}

vk::Pipeline PipelineCache::retrieve_pipeline(VKContext &context, SceGxmPrimitiveType &type, MemState &mem) {
    current_context = &context;
    const GxmRecordState &record = context.record;
    // get the hash of the current context
    constexpr size_t record_pipeline_len = offsetof(GxmRecordState, vertex_streams);
    uint64_t key = XXH_INLINE_XXH3_64bits(&record, record_pipeline_len);

    // add the hash of the blending
    const SceGxmFragmentProgram &fragment_program_gxm = *record.fragment_program.get(mem);
    const VKFragmentProgram &fragment_program = *reinterpret_cast<VKFragmentProgram *>(
        fragment_program_gxm.renderer_data.get());
    key ^= fragment_program.blending_hash;

    // add the hash of the attribute and stream layout
    const SceGxmVertexProgram &vertex_program_gxm = *record.vertex_program.get(mem);
    key ^= vertex_program_gxm.key_hash;

    // and also add the primitive type
    key ^= static_cast<uint64_t>(type);
    auto it = pipelines.find(key);
    if (it != pipelines.end())
        return it->second;

    const VertexProgram &vertex_program = *reinterpret_cast<VertexProgram *>(
        vertex_program_gxm.renderer_data.get());
    const SceGxmProgram *gxm_fragment_shader = fragment_program_gxm.program.get(mem);

    // the vertex input state must be computed before shader are retrieved in case symbols are stripped
    const vk::PipelineVertexInputStateCreateInfo vertex_input = get_vertex_input_state(mem);

    const vk::PipelineShaderStageCreateInfo vertex_shader = retrieve_shader(vertex_program_gxm.program.get(mem), vertex_program.hash, true, fragment_program_gxm.is_maskupdate, mem, &vertex_program_gxm.attributes);
    const vk::PipelineShaderStageCreateInfo fragment_shader = retrieve_shader(gxm_fragment_shader, fragment_program.hash, false, fragment_program_gxm.is_maskupdate, mem, nullptr);
    const vk::PipelineShaderStageCreateInfo shader_stages[] = { vertex_shader, fragment_shader };
    // disable the fragment shader if gxm asks us to
    const bool is_fragment_disabled = record.front_side_fragment_program_mode == SCE_GXM_FRAGMENT_PROGRAM_DISABLED;
    const uint32_t shader_stage_count = is_fragment_disabled ? 1U : 2U;

    const vk::PipelineInputAssemblyStateCreateInfo input_assembly{
        .topology = translate_primitive(type)
    };

    const bool two_sided = (record.two_sided == SCE_GXM_TWO_SIDED_ENABLED);

    const bool use_shader_interlock = state.features.support_shader_interlock && gxm_fragment_shader->is_frag_color_used();

    const vk::PipelineRasterizationStateCreateInfo rasterizer{
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
    if (is_fragment_disabled || use_shader_interlock) {
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

    vk::PipelineLayout pipeline_layout = retrieve_pipeline_layout(vertex_program.texture_count, fragment_program.texture_count);

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

    // we still need to specifiy the viewport and scissor count even though they are dynamic
    vk::PipelineViewportStateCreateInfo viewport{
        .viewportCount = 1,
        .scissorCount = 1
    };

    const vk::RenderPass render_pass = use_shader_interlock ? context.current_shader_interlock_pass : context.current_render_pass;
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

    pipelines[key] = result.value;

    return result.value;
}

bool PipelineCache::precompile_shader(const Sha256Hash &hash) {
    const auto shader_path{ fs::path(state.base_path) / "cache/shaders" / state.title_id / state.self_name };

    if (shaders.contains(hash))
        return true;

    if (!fs::exists(shader_path) || fs::is_empty(shader_path))
        return false;

    Sha256Hash shader_hash;
    memcpy(shader_hash.data(), hash.data(), sizeof(Sha256Hash));
    const std::string hash_ver = fmt::format("vk{}-{}", shader::CURRENT_VERSION, hex_string(shader_hash));

    const std::vector<uint32_t> source = renderer::pre_load_shader_spirv(hash_ver.c_str(), "spv", state.base_path, state.title_id, state.self_name);

    if (source.empty())
        return false;

    vk::ShaderModuleCreateInfo shader_info{
        .codeSize = sizeof(uint32_t) * source.size(),
        .pCode = source.data()
    };

    vk::ShaderModule shader = state.device.createShaderModule(shader_info);
    shaders[hash] = shader;

    return true;
}
} // namespace renderer::vulkan