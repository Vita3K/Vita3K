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

#include <codec/state.h>

extern "C" {
#include <libavcodec/avcodec.h>
}

#include <cassert>

uint32_t DecoderState::get(DecoderQuery query) {
    return 0;
}

void DecoderState::configure(void *options) {
    // do nothing
}

uint32_t DecoderState::get_es_size(const uint8_t *data) {
    return std::numeric_limits<uint32_t>::max();
}

void DecoderState::flush() {
    avcodec_flush_buffers(context);
}

DecoderState::~DecoderState() {
    avcodec_close(context);
    avcodec_free_context(&context);
}
