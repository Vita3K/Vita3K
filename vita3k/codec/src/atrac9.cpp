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
#include <libavutil/opt.h>
#include <libswresample/swresample.h>
}

#include <error_codes.h>
#include <libatrac9.h>

#include <util/log.h>

#include <cassert>

struct FFMPEGAtrac9Info {
    uint32_t version;
    uint32_t config_data;
    uint32_t padding;
};

bool resample_s16_to_f32(const int16_t *source_s16, int32_t source_channels, uint32_t source_samples, uint32_t source_freq,
    float *dest_f32, uint32_t dest_samples, uint32_t dest_freq) {
    const int source_channel_type = source_channels == 2 ? AV_CH_LAYOUT_STEREO : AV_CH_LAYOUT_MONO;

    SwrContext *swr = swr_alloc_set_opts(nullptr,
        AV_CH_LAYOUT_STEREO, AV_SAMPLE_FMT_FLT, dest_freq,
        source_channel_type, AV_SAMPLE_FMT_S16, source_freq,
        0, nullptr);

    swr_init(swr);

    const int result = swr_convert(swr, (uint8_t **)&dest_f32, dest_samples, (const uint8_t **)(&source_s16), source_samples);
    swr_free(&swr);
    assert(result > 0);
    return (result >= 0);
};

/*
void resample_f32(const int16_t *f32_s, float *f32_d, uint32_t source_channels, uint32_t samples, uint32_t freq) {
    resample_s16_to_f32(f32_s, source_channels, samples, freq, f32_d, samples, freq);
}
*/
// maybe turn these back into public funcs thanks to DecoderQuery?
uint32_t Atrac9DecoderState::get_channel_count() {
    const std::uint8_t block_rate_index = ((config_data & (0b111 << 9)) >> 9);
    std::uint32_t total_channels = 0;

    switch (block_rate_index) {
    case 0: // Mono
        total_channels = 1;
        break;

    case 1: // Dual Mono (Mono, Mono)
        total_channels = 2;
        break;

    case 2: // Stereo
        total_channels = 2;
        break;

    case 3: // Stereo, Mono, LFE, Stereo
        total_channels = 2 + 1 + 1 + 2;
        break;

    case 4: // Stereo, Mono, LFE, Stereo, Stereo
        total_channels = 2 + 1 + 1 + 2 + 2;
        break;

    case 5: // Dual Stereo
        total_channels = 4;
        break;

    default:
        total_channels = 2;
        break;
    }

    return total_channels;
}

uint32_t Atrac9DecoderState::get_samples_per_superframe() {
    const std::uint8_t superframe_index = (config_data & (0b11 << 27)) >> 27;
    const std::uint8_t sample_rate_index = ((config_data & (0b1111 << 12)) >> 12);
    const std::uint32_t frame_per_superframe = 1 << superframe_index;

    static const std::int8_t sample_rate_index_to_frame_sample_power[] = {
        6, 6, 7, 7, 7, 8, 8, 8, 6, 6, 7, 7, 7, 8, 8, 8
    };

    const std::uint32_t samples_per_frame = 1 << sample_rate_index_to_frame_sample_power[sample_rate_index];
    const std::uint32_t samples_per_superframe = samples_per_frame * frame_per_superframe;

    return samples_per_superframe;
}

uint32_t Atrac9DecoderState::get_block_align() {
    // How many channel presents?
    return get_samples_per_superframe() * 4 * get_channel_count();
}

uint32_t Atrac9DecoderState::get_frames_in_superframe() {
    const std::uint8_t superframe_index = (config_data & (0b11 << 27)) >> 27;

    return 1 << superframe_index;
}

uint32_t Atrac9DecoderState::get_superframe_size() {
    const std::uint16_t frame_bytes = ((((config_data & 0xFF0000) >> 16) << 3) | ((config_data & (0b111 << 29)) >> 29)) + 1;
    return frame_bytes * get_frames_in_superframe();
}

uint32_t Atrac9DecoderState::get(DecoderQuery query) {
    Atrac9CodecInfo info;
    Atrac9GetCodecInfo(decoder_handle, &info);

    switch (query) {
    case DecoderQuery::CHANNELS: return info.channels;
    case DecoderQuery::BIT_RATE: return 0;
    case DecoderQuery::SAMPLE_RATE: return info.samplingRate;
    case DecoderQuery::AT9_BLOCK_ALIGN: return get_block_align();
    case DecoderQuery::AT9_SAMPLE_PER_SUPERFRAME: return get_samples_per_superframe();
    case DecoderQuery::AT9_FRAMES_IN_SUPERFRAME: return get_frames_in_superframe();
    case DecoderQuery::AT9_SUPERFRAME_SIZE: return get_superframe_size();
    default: return 0;
    }
}

bool Atrac9DecoderState::send(const uint8_t *data, uint32_t size) {
    result.clear();

    Atrac9CodecInfo info;
    Atrac9GetCodecInfo(decoder_handle, &info);

    int produced_time = 0;
    std::uint32_t size_used = 0;
    std::uint32_t produce_size = info.frameSamples * info.channels * sizeof(uint16_t);

    while ((size_used < size) && (produced_time < info.framesInSuperframe)) {
        int decode_used = 0;

        result.resize(++produced_time * produce_size);
        const int res = Atrac9Decode(decoder_handle, data, reinterpret_cast<short *>(result.data() + ((produced_time - 1) * produce_size)), &decode_used);
        if (res != At9Status::ERR_SUCCESS) {
            LOG_ERROR("Decode failure with code {}!", res);
            return false;
        }

        data += decode_used;
        size_used += decode_used;
    }

    return true;
}

bool Atrac9DecoderState::receive(uint8_t *data, DecoderSize *size) {
    if (data) {
        memcpy(data, result.data(), result.size());
    }

    if (size) {
        Atrac9CodecInfo info;
        Atrac9GetCodecInfo(decoder_handle, &info);
        std::uint32_t sample_decoded = static_cast<std::uint32_t>(result.size() / (info.channels * sizeof(uint16_t)));
        size->samples = sample_decoded;
    }

    return true;
}

Atrac9DecoderState::Atrac9DecoderState(uint32_t config_data)
    : config_data(config_data) {
    decoder_handle = Atrac9GetHandle();
    const int result = Atrac9InitDecoder(decoder_handle, reinterpret_cast<uint8_t *>(&config_data));

    if (result != At9Status::ERR_SUCCESS) {
        LOG_ERROR("Error initializing decoder!");
    }
}

Atrac9DecoderState::~Atrac9DecoderState() {
    Atrac9ReleaseHandle(decoder_handle);
    context = nullptr;
}
