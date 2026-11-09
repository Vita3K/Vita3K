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

#include "shader_translate.h"

#include <util/log.h>

#include <spirv_msl.hpp>

#include <fstream>

namespace renderer::metal::msl {

using namespace spirv_cross;

static spv::ExecutionModel to_execution_model(Stage stage) {
    switch (stage) {
    case Stage::Vertex:
        return spv::ExecutionModelVertex;
    case Stage::Fragment:
        return spv::ExecutionModelFragment;
    default:
        return spv::ExecutionModelGLCompute;
    }
}

static void set_options(CompilerMSL &compiler, bool is_vertex, bool use_framebuffer_fetch) {
    CompilerMSL::Options options;
    options.platform = CompilerMSL::Options::macOS;
    options.set_msl_version(2, 3);
    // bindings are explicitly remapped
    options.enable_decoration_binding = false;
    options.pad_fragment_output_components = true;
    options.texture_1D_as_2D = true;
    options.swizzle_buffer_index = SWIZZLE_BUFFER_INDEX;
    options.buffer_size_buffer_index = BUFFER_SIZE_BUFFER_INDEX;
    options.use_framebuffer_fetch_subpasses = use_framebuffer_fetch;
    options.enable_point_size_builtin = true;
    options.argument_buffers = false;
    compiler.set_msl_options(options);

    CompilerGLSL::Options common_options = compiler.get_common_options();
    // Metal clip space is y-up while the shaders are generated for Vulkan
    common_options.vertex.flip_vert_y = is_vertex;
    common_options.vertex.fixup_clipspace = false;
    compiler.set_common_options(common_options);
}

static void add_binding(CompilerMSL &compiler, spv::ExecutionModel model, uint32_t set, uint32_t binding, uint32_t buffer, uint32_t texture, uint32_t sampler) {
    MSLResourceBinding resource;
    resource.stage = model;
    resource.desc_set = set;
    resource.binding = binding;
    resource.count = 1;
    resource.msl_buffer = buffer;
    resource.msl_texture = texture;
    resource.msl_sampler = sampler;
    compiler.add_msl_resource_binding(resource);
}

static TranslateResult compile(CompilerMSL &compiler, const char *entry_point, spv::ExecutionModel model) {
    TranslateResult result;
    try {
        compiler.set_entry_point(entry_point, model);
        result.source = compiler.compile();
        result.entry_point = compiler.get_cleansed_entry_point_name(entry_point, model);
        result.ok = true;
    } catch (const CompilerError &e) {
        result.ok = false;
        result.error = e.what();
    } catch (const std::exception &e) {
        result.ok = false;
        result.error = e.what();
    }
    return result;
}

TranslateResult translate_gxm_shader(const std::vector<uint32_t> &spirv, bool is_vertex, bool use_framebuffer_fetch) {
    try {
        CompilerMSL compiler(spirv);
        set_options(compiler, is_vertex, use_framebuffer_fetch);

        const spv::ExecutionModel model = is_vertex ? spv::ExecutionModelVertex : spv::ExecutionModelFragment;
        if (is_vertex) {
            // set 0: render info uniform buffer (binding 0) and gxm uniform storage buffer (binding 2)
            add_binding(compiler, model, 0, 0, VERTEX_RENDER_INFO_BUFFER_INDEX, 0, 0);
            add_binding(compiler, model, 0, 2, VERTEX_GXM_UNIFORM_BUFFER_INDEX, 0, 0);
            // set 2: vertex textures
            for (uint32_t i = 0; i < 16; i++)
                add_binding(compiler, model, 2, i, 0, i, i);
        } else {
            add_binding(compiler, model, 0, 1, FRAGMENT_RENDER_INFO_BUFFER_INDEX, 0, 0);
            add_binding(compiler, model, 0, 3, FRAGMENT_GXM_UNIFORM_BUFFER_INDEX, 0, 0);
            // set 1: color attachment, mask, raw color attachment
            add_binding(compiler, model, 1, 0, 0, COLOR_ATTACHMENT_TEXTURE_INDEX, 0);
            add_binding(compiler, model, 1, 1, 0, MASK_TEXTURE_INDEX, 0);
            add_binding(compiler, model, 1, 2, 0, COLOR_ATTACHMENT_RAW_TEXTURE_INDEX, 0);
            // set 3: fragment textures
            for (uint32_t i = 0; i < 16; i++)
                add_binding(compiler, model, 3, i, 0, i, i);
        }

        return compile(compiler, is_vertex ? "main_vs" : "main_fs", model);
    } catch (const std::exception &e) {
        TranslateResult result;
        result.ok = false;
        result.error = e.what();
        return result;
    }
}

TranslateResult translate_builtin_shader(const std::vector<uint32_t> &spirv, Stage stage, const std::vector<BindingRemap> &remaps, uint32_t push_constant_buffer_index) {
    try {
        CompilerMSL compiler(spirv);
        set_options(compiler, stage == Stage::Vertex, false);

        const spv::ExecutionModel model = to_execution_model(stage);
        for (const BindingRemap &remap : remaps)
            add_binding(compiler, model, remap.set, remap.binding, remap.msl_buffer, remap.msl_texture, remap.msl_sampler);
        add_binding(compiler, model, kPushConstDescSet, kPushConstBinding, push_constant_buffer_index, 0, 0);

        return compile(compiler, "main", model);
    } catch (const std::exception &e) {
        TranslateResult result;
        result.ok = false;
        result.error = e.what();
        return result;
    }
}

std::vector<uint32_t> load_spirv_file(const fs::path &path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file)
        return {};

    const auto size = file.tellg();
    if (size <= 0)
        return {};

    std::vector<uint32_t> code((static_cast<size_t>(size) + 3) / 4, 0);
    file.seekg(0);
    file.read(reinterpret_cast<char *>(code.data()), size);
    return code;
}

} // namespace renderer::metal::msl
