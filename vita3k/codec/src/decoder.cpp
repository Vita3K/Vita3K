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

#include <codec/state.h>

extern "C" {
#include <libavcodec/avcodec.h>
}

#include <util/log.h>

uint32_t DecoderState::get(DecoderQuery query) {
    return 0;
}

uint32_t DecoderState::get_es_size() {
    return std::numeric_limits<uint32_t>::max();
}

void DecoderState::flush() {
    std::lock_guard<std::mutex> lock(codec_mutex);
    if (context)
        avcodec_flush_buffers(context);
}

DecoderState::~DecoderState() {
    avcodec_free_context(&context);
}

// Handy to have this in logs, some debuggers don't seem to be able to evaluate there error macros properly.
std::string codec_error_name(int error) {
    switch (error) {
    case AVERROR(EAGAIN):
        return "Requires Another Call (AVERROR(EAGAIN))";
    case AVERROR_EOF:
        return "End of File (AVERROR_EOF)";
    case AVERROR(EINVAL):
        return "Invalid Call (AVERROR(EINVAL))";
    case AVERROR(ENOMEM):
        return "Out of Memory (AVERROR(ENOMEM))";
    case AVERROR_INVALIDDATA:
        return "Invalid Data (AVERROR_INVALIDDATA)";
    default:
        return log_hex(static_cast<uint32_t>(error));
    }
}
