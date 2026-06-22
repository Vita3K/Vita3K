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

#pragma once

#include <ngs/system.h>

struct SwrContext;

namespace ngs {

struct StereoRateResamplerLogicalState {
    PCMFrameQueue input_history;
    bool needs_reset = false;

    void clear() {
        input_history.clear();
        needs_reset = false;
    }

    void reset() {
        input_history.clear();
        needs_reset = true;
    }
};

struct StereoRateResamplerRuntimeState {
    SwrContext *context = nullptr;
    int source_rate = 0;
    int dest_rate = 0;
    std::vector<uint8_t> scratch_buffer;
};

void destroy_stereo_rate_resampler(StereoRateResamplerRuntimeState &runtime);
bool ensure_stereo_rate_resampler(StereoRateResamplerRuntimeState &runtime, StereoRateResamplerLogicalState &logical,
    int source_rate, int dest_rate);
uint32_t process_stereo_rate_resampler(StereoRateResamplerRuntimeState &runtime, StereoRateResamplerLogicalState &logical,
    const uint8_t *input, uint32_t input_frames, PCMFrameQueue &output);

} // namespace ngs
