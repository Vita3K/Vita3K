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

    const int base_binding_point = vertex_shader ? 0 : 1;
    const int base_binding_ubo_relative = vertex_shader ? 0 : (SCE_GXM_REAL_MAX_UNIFORM_BUFFER + 1);

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, context.ssbo[base_binding_point]);
    glBufferSubData(GL_SHADER_STORAGE_BUFFER, offset * 4, std::min<GLsizeiptr>(size, program->uniform_buffer_sizes.at(block_num) * 4), data);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, base_binding_point, context.ssbo[base_binding_point]);

    if (log_active_shader) {
        std::vector<uint8_t> my_data((uint8_t *)data, (uint8_t *)data + size);
        context.ubo_data[base_binding_ubo_relative + block_num] = my_data;
    }

    return true;
}
} // namespace renderer::gl
