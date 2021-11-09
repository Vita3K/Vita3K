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
bool set_uniform_buffer(GLContext &context, const bool vertex_shader, const int block_num, const int size, const void *data, bool log_active_shader, bool is_ssbo) {
    const int binding_base = vertex_shader ? 0 : (SCE_GXM_REAL_MAX_UNIFORM_BUFFER + 1);
    GLenum buffer_type = (is_ssbo ? GL_SHADER_STORAGE_BUFFER : GL_UNIFORM_BUFFER);

    glBindBuffer(buffer_type, context.uniform_buffer[binding_base + block_num]);
    glBufferData(buffer_type, size, nullptr, GL_DYNAMIC_COPY);
    glBufferData(buffer_type, size, data, GL_DYNAMIC_COPY);
    glBindBuffer(buffer_type, 0);

    glBindBufferBase(buffer_type, binding_base + block_num, context.uniform_buffer[binding_base + block_num]);

    if (log_active_shader) {
        std::vector<uint8_t> my_data((uint8_t *)data, (uint8_t *)data + size);
        context.ubo_data[binding_base + block_num] = my_data;
    }

    return true;
}
} // namespace renderer::gl
