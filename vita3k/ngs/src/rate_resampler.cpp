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

#include <ngs/rate_resampler.h>
#include <util/log.h>

extern "C" {
#include <libavutil/channel_layout.h>
#include <libavutil/samplefmt.h>
#include <libswresample/swresample.h>
}

#include <algorithm>
#include <cstdint>

namespace ngs {
namespace {
constexpr uint32_t stereo_channels = 2;
constexpr uint32_t history_safety_margin_frames = 128;
constexpr uint32_t history_compact_threshold_frames = 1024;

bool create_stereo_rate_resampler(StereoRateResamplerRuntimeState &runtime, const int source_rate,
    const int dest_rate) {
    AVChannelLayout stereo = AV_CHANNEL_LAYOUT_STEREO;
    const int alloc_result = swr_alloc_set_opts2(&runtime.context,
        &stereo, AV_SAMPLE_FMT_FLT, dest_rate,
        &stereo, AV_SAMPLE_FMT_FLT, source_rate,
        0, nullptr);
    if (alloc_result < 0) {
        LOG_ERROR("Failed to allocate stereo rate SwrContext for {} -> {} Hz (error {}).",
            source_rate, dest_rate, alloc_result);
        runtime.context = nullptr;
        return false;
    }

    const int init_result = swr_init(runtime.context);
    if (init_result < 0) {
        LOG_ERROR("Failed to initialize stereo rate SwrContext for {} -> {} Hz (error {}).",
            source_rate, dest_rate, init_result);
        swr_free(&runtime.context);
        return false;
    }

    runtime.source_rate = source_rate;
    runtime.dest_rate = dest_rate;
    return true;
}

void compact_history_if_needed(StereoRateResamplerLogicalState &logical) {
    if (logical.input_history.read_offset_frames == 0) {
        return;
    }

    const uint32_t total_frames = logical.input_history.total_frames();
    if (logical.input_history.read_offset_frames >= history_compact_threshold_frames
        || logical.input_history.read_offset_frames >= (total_frames / 2)) {
        logical.input_history.compact();
    }
}

void trim_history(StereoRateResamplerRuntimeState &runtime, StereoRateResamplerLogicalState &logical) {
    if (!runtime.context) {
        logical.input_history.clear();
        return;
    }

    const int64_t delay = swr_get_delay(runtime.context, runtime.source_rate);
    const uint32_t keep_frames = static_cast<uint32_t>(std::max<int64_t>(delay, 0)) + history_safety_margin_frames;
    const uint32_t available_frames = logical.input_history.available_frames();

    if (available_frames > keep_frames) {
        logical.input_history.consume_frames(available_frames - keep_frames);
        compact_history_if_needed(logical);
    }
}

bool replay_history(StereoRateResamplerRuntimeState &runtime, StereoRateResamplerLogicalState &logical) {
    logical.input_history.compact();

    const uint32_t history_frames = logical.input_history.available_frames();
    if (history_frames == 0) {
        return true;
    }

    const int out_samples = swr_get_out_samples(runtime.context, static_cast<int>(history_frames));
    if (out_samples < 0) {
        LOG_ERROR("Failed to query stereo rate resampler replay output size for {} history frames (error {}).",
            history_frames, out_samples);
        return false;
    }

    runtime.scratch_buffer.resize(static_cast<size_t>(out_samples) * sizeof(float) * stereo_channels);

    uint8_t *discard_output = runtime.scratch_buffer.empty() ? nullptr : runtime.scratch_buffer.data();
    const uint8_t *history_input = logical.input_history.read_bytes();
    const int result = swr_convert(runtime.context, &discard_output, out_samples, &history_input,
        static_cast<int>(history_frames));
    if (result < 0) {
        LOG_ERROR("Failed to replay {} history frames into stereo rate resampler (error {}).",
            history_frames, result);
        return false;
    }
    return true;
}
} // namespace

void destroy_stereo_rate_resampler(StereoRateResamplerRuntimeState &runtime) {
    if (runtime.context) {
        swr_free(&runtime.context);
    }

    runtime.source_rate = 0;
    runtime.dest_rate = 0;
    runtime.scratch_buffer.clear();
}

bool ensure_stereo_rate_resampler(StereoRateResamplerRuntimeState &runtime, StereoRateResamplerLogicalState &logical,
    const int source_rate, const int dest_rate) {
    if (source_rate <= 0 || dest_rate <= 0) {
        LOG_ERROR("Invalid stereo rate resampler configuration {} -> {} Hz.", source_rate, dest_rate);
        return false;
    }

    const bool needs_recreate = logical.needs_reset || !runtime.context || runtime.source_rate != source_rate
        || runtime.dest_rate != dest_rate;

    if (!needs_recreate) {
        return true;
    }

    destroy_stereo_rate_resampler(runtime);
    if (!create_stereo_rate_resampler(runtime, source_rate, dest_rate)) {
        return false;
    }

    if (!replay_history(runtime, logical)) {
        destroy_stereo_rate_resampler(runtime);
        return false;
    }

    logical.needs_reset = false;
    return true;
}

uint32_t process_stereo_rate_resampler(StereoRateResamplerRuntimeState &runtime, StereoRateResamplerLogicalState &logical,
    const uint8_t *input, const uint32_t input_frames, PCMFrameQueue &output) {
    if (!input || input_frames == 0 || !runtime.context) {
        return 0;
    }

    const int out_samples = swr_get_out_samples(runtime.context, static_cast<int>(input_frames));
    if (out_samples < 0) {
        LOG_ERROR("Failed to query stereo rate resampler output size for {} input frames (error {}).",
            input_frames, out_samples);
        return 0;
    }

    const size_t old_samples = output.samples.size();
    output.samples.resize(old_samples + static_cast<size_t>(out_samples) * stereo_channels);

    uint8_t *output_bytes = reinterpret_cast<uint8_t *>(output.samples.data() + old_samples);
    const uint8_t *input_bytes = input;
    const int produced_samples = swr_convert(runtime.context, &output_bytes, out_samples, &input_bytes,
        static_cast<int>(input_frames));

    if (produced_samples < 0) {
        LOG_ERROR("Stereo rate resampler failed while converting {} input frames (error {}).",
            input_frames, produced_samples);
        output.samples.resize(old_samples);
        return 0;
    }

    output.samples.resize(old_samples + static_cast<size_t>(produced_samples) * stereo_channels);

    logical.input_history.append_bytes(input, input_frames);
    trim_history(runtime, logical);

    return static_cast<uint32_t>(produced_samples);
}

} // namespace ngs
