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

// NOTE: I revised algorithm from RPCS3's GLHelpers.h. Please look at the class ring_buffer. Asked kd for advice.
// I (pent0) mostly rewritten to understand it, other contributors in future can see the rpcs3 code.

#include <renderer/gl/ring_buffer.h>
#include <util/align.h>
#include <util/log.h>

namespace renderer::gl {

RingBuffer::RingBuffer(GLenum purpose, const std::size_t capacity)
    : base_(nullptr)
    , cursor_(0)
    , capacity_(capacity)
    , purpose_(purpose) {
    buffer_.init(reinterpret_cast<renderer::Generator *>(glGenBuffers), reinterpret_cast<renderer::Deleter *>(glDeleteBuffers));
}

RingBuffer::~RingBuffer() {
    if (base_) {
        glBindBuffer(purpose_, buffer_[0]);
        glUnmapBuffer(purpose_);
    }
}

void RingBuffer::create_and_map() {
    glBindBuffer(purpose_, buffer_[0]);
    glBufferStorage(purpose_, capacity_, nullptr, GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT);

    base_ = reinterpret_cast<std::uint8_t *>(glMapBufferRange(purpose_, 0, capacity_, GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT));

    if (!base_) {
        LOG_ERROR("Failed to map persistent buffer to host!");
    }
}

std::pair<std::uint8_t *, std::size_t> RingBuffer::allocate(const std::size_t data_size) {
    if (!base_) {
        create_and_map();
        if (!base_) {
            return std::make_pair(nullptr, static_cast<std::size_t>(-1));
        }
    }

    std::size_t offset = align(cursor_, 256);

    if ((offset + data_size) >= capacity_) {
        if (!safe_segment_consumed_fence_.empty()) {
            safe_segment_consumed_fence_.wait_for_signal();
        } else {
            LOG_ERROR("Sync fence has not been inserted but the size of the ring buffer has already reached full!");
            glFinish();
        }

        offset = 0;
        cursor_ = 0;
    }

    cursor_ = align(offset + data_size, 256);
    return std::make_pair(base_ + offset, offset);
}

void RingBuffer::draw_call_done() {
    // Insert a fence when the buffer has been consumed about 25% (same as RPCS3)
    if (safe_segment_consumed_fence_.empty() && (cursor_ >= (capacity_ >> 2))) {
        safe_segment_consumed_fence_.insert();
    }
}

} // namespace renderer::gl
