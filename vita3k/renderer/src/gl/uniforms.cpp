// Vita3K emulator project
// Copyright (C) 2021 Vita3K team
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

#include <renderer/gl/functions.h>
#include <renderer/gl/types.h>

#include <renderer/profile.h>
#include <renderer/types.h>

#include <algorithm>

namespace renderer::gl {
bool set_uniform_buffer(GLContext &context, MemState &mem, const bool vertex_shader, const int block_num, const int size, const void *data, bool log_active_shader) {
    renderer::ShaderProgram *program = vertex_shader ? reinterpret_cast<renderer::ShaderProgram *>(context.record.vertex_program.get(mem)->renderer_data.get())
                                                     : reinterpret_cast<renderer::ShaderProgram *>(context.record.fragment_program.get(mem)->renderer_data.get());

    auto offset = program->uniform_buffer_data_offsets.at(block_num);
    if (offset == static_cast<std::uint32_t>(-1)) {
        return true;
    }

    if (vertex_shader) {
        if (!context.vertex_uniform_buffer_storage_ptr.first) {
            // Allocate a region for it. Don't worry though, when the shader program is changed
            context.vertex_uniform_buffer_storage_ptr = context.vertex_uniform_stream_ring_buffer.allocate(program->max_total_uniform_buffer_storage * 4);

            if (!context.vertex_uniform_buffer_storage_ptr.first) {
                LOG_ERROR("Unable to allocate vertex SSBO from persistent mapped buffer");
                return false;
            }

            glBindBufferRange(GL_SHADER_STORAGE_BUFFER, 0, context.vertex_uniform_stream_ring_buffer.handle(), context.vertex_uniform_buffer_storage_ptr.second, program->max_total_uniform_buffer_storage * 4);
        }
    } else {
        if (!context.fragment_uniform_buffer_storage_ptr.first) {
            // Allocate a region for it. Don't worry though, when the shader program is changed
            context.fragment_uniform_buffer_storage_ptr = context.fragment_uniform_stream_ring_buffer.allocate(program->max_total_uniform_buffer_storage * 4);

            if (!context.fragment_uniform_buffer_storage_ptr.first) {
                LOG_ERROR("Unable to allocate fragment SSBO from persistent mapped buffer");
                return false;
            }

            glBindBufferRange(GL_SHADER_STORAGE_BUFFER, 1, context.fragment_uniform_stream_ring_buffer.handle(), context.fragment_uniform_buffer_storage_ptr.second, program->max_total_uniform_buffer_storage * 4);
        }
    }

    const int base_binding_ubo_relative = vertex_shader ? 0 : (SCE_GXM_REAL_MAX_UNIFORM_BUFFER + 1);
    const std::size_t data_size_upload = std::min<GLsizeiptr>(size, program->uniform_buffer_sizes.at(block_num) * 4);
    const std::size_t offset_start_upload = offset * 4;

    if (vertex_shader) {
        std::memcpy(context.vertex_uniform_buffer_storage_ptr.first + offset_start_upload, data, data_size_upload);
    } else {
        std::memcpy(context.fragment_uniform_buffer_storage_ptr.first + offset_start_upload, data, data_size_upload);
    }

    if (log_active_shader) {
        std::vector<uint8_t> my_data((uint8_t *)data, (uint8_t *)data + size);
        context.ubo_data[base_binding_ubo_relative + block_num] = my_data;
    }

    return true;
}
} // namespace renderer::gl
