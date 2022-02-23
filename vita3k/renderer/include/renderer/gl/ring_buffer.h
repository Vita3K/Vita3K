// Vita3K emulator project
// Copyright (C) 2022 Vita3K team
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

#include <cstdint>
#include <glutil/object_array.h>
#include <renderer/gl/fence.h>
#include <tuple>

namespace renderer::gl {

struct RingBuffer {
private:
    GLObjectArray<1> buffer_;
    Fence safe_segment_consumed_fence_;

    std::uint8_t *base_;
    std::size_t cursor_;
    std::size_t capacity_;

    GLenum purpose_;

    void create_and_map();

public:
    explicit RingBuffer(GLenum purpose, const std::size_t capacity);
    ~RingBuffer();

    // Allocate new data from ring buffer, return offset of the data resided in the buffer
    // In case the data is full, it will wait for fence to notify previous draw commands has finished on previous buffer part
    // and then reset the cursor
    std::pair<std::uint8_t *, std::size_t> allocate(const std::size_t data_size);

    // Notify the buffer that a draw call is done. This inserts a fence depends on the amount of data that has been consumed
    // previously by push
    void draw_call_done();

    GLint handle() const {
        return buffer_[0];
    }
};

} // namespace renderer::gl
