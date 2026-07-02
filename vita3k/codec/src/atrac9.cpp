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

#include <codec/state.h>

extern "C" {
#include <libatrac9.h>
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
#include <libswresample/swresample.h>
#include <structures.h>
}

#include <error_codes.h>

#include <util/log.h>

#include <algorithm>

struct FFMPEGAtrac9Info {
    uint32_t version;
    uint32_t config_data;
    uint32_t padding;
};

uint32_t Atrac9DecoderState::get(DecoderQuery query) {
    Atrac9CodecInfo *info = static_cast<Atrac9CodecInfo *>(atrac9_info);
    if (!info) {
        return 0;
    }

    switch (query) {
    case DecoderQuery::CHANNELS: return info->channels;
    // The bit rate is the size of a superframe times the number of superframes per second (times 8)
    case DecoderQuery::BIT_RATE: {
        const int samples_per_superframe = info->frameSamples * info->framesInSuperframe;
        if (samples_per_superframe <= 0) {
            LOG_ERROR("Invalid Atrac9 codec info when querying bitrate.");
            return 0;
        }

        return static_cast<uint32_t>((info->superframeSize * 8ULL * info->samplingRate) / samples_per_superframe);
    }
    case DecoderQuery::SAMPLE_RATE: return info->samplingRate;
    case DecoderQuery::AT9_SAMPLE_PER_FRAME: return info->frameSamples;
    case DecoderQuery::AT9_SAMPLE_PER_SUPERFRAME: return info->frameSamples * info->framesInSuperframe;
    case DecoderQuery::AT9_FRAMES_IN_SUPERFRAME: return info->framesInSuperframe;
    case DecoderQuery::AT9_SUPERFRAME_SIZE: return info->superframeSize;
    default: return 0;
    }
}

uint32_t Atrac9DecoderState::get_es_size() {
    return es_size_used;
}

bool Atrac9DecoderState::is_initialized() const {
    return initialized;
}

void Atrac9DecoderState::flush() {
    if (!initialized) {
        return;
    }

    Atrac9CodecInfo *info = static_cast<Atrac9CodecInfo *>(atrac9_info);
    superframe_frame_idx = 0;
    superframe_data_left = info->superframeSize;

    Frame &frame = static_cast<Atrac9Handle *>(decoder_handle)->Frame;
    frame.IndexInSuperframe = 0;
    if (frame.Channels[0])
        std::fill_n(frame.Channels[0]->Mdct.ImdctPrevious, 256, 0.0);
    if (frame.Channels[1])
        std::fill_n(frame.Channels[1]->Mdct.ImdctPrevious, 256, 0.0);
}

void Atrac9DecoderState::export_state(Atrac9DecoderSavedState *dest) {
    if (!initialized) {
        return;
    }

    Frame &frame = static_cast<Atrac9Handle *>(decoder_handle)->Frame;
    if (frame.Channels[0])
        std::copy_n(frame.Channels[0]->Mdct.ImdctPrevious, 256, dest->prev_values[0]);
    if (frame.Channels[1])
        std::copy_n(frame.Channels[1]->Mdct.ImdctPrevious, 256, dest->prev_values[1]);
}

void Atrac9DecoderState::load_state(const Atrac9DecoderSavedState *src) {
    if (!initialized) {
        return;
    }

    Frame &frame = static_cast<Atrac9Handle *>(decoder_handle)->Frame;
    if (frame.Channels[0])
        std::copy_n(src->prev_values[0], 256, frame.Channels[0]->Mdct.ImdctPrevious);
    if (frame.Channels[1])
        std::copy_n(src->prev_values[1], 256, frame.Channels[1]->Mdct.ImdctPrevious);
}

bool Atrac9DecoderState::send(const uint8_t *data, uint32_t size) {
    if (!initialized) {
        return false;
    }

    Atrac9CodecInfo *info = static_cast<Atrac9CodecInfo *>(atrac9_info);

    int decode_used = 0;

    const int res = Atrac9Decode(decoder_handle, data, reinterpret_cast<short *>(result.data()), &decode_used);
    if (res != At9Status::ERR_SUCCESS) {
        LOG_ERROR("Decode failure with code {}", log_hex(res));
        return false;
    }

    es_size_used = static_cast<uint32_t>(decode_used);
    superframe_data_left -= decode_used;
    superframe_frame_idx++;
    if (superframe_frame_idx == info->framesInSuperframe) {
        // add the padding between two superframes as size used if there is
        es_size_used += superframe_data_left;
        superframe_frame_idx = 0;
        superframe_data_left = info->superframeSize;
    }

    return true;
}

bool Atrac9DecoderState::receive(uint8_t *data, DecoderSize *size) {
    if (!initialized) {
        return false;
    }

    Atrac9CodecInfo *info = static_cast<Atrac9CodecInfo *>(atrac9_info);

    if (data) {
        memcpy(data, result.data(), info->frameSamples * info->channels * sizeof(uint16_t));
    }

    if (size) {
        size->samples = static_cast<uint32_t>(info->frameSamples);
    }

    return true;
}

Atrac9DecoderState::Atrac9DecoderState(uint32_t config_data)
    : config_data(config_data)
    , decoder_handle(Atrac9GetHandle())
    , atrac9_info(new Atrac9CodecInfo{}) {
    const int err = Atrac9InitDecoder(decoder_handle, reinterpret_cast<uint8_t *>(&config_data));

    if (err != At9Status::ERR_SUCCESS) {
        LOG_ERROR("Error initializing decoder. Error code: {}", log_hex(err));
        return;
    }

    Atrac9CodecInfo *info = static_cast<Atrac9CodecInfo *>(atrac9_info);
    Atrac9GetCodecInfo(decoder_handle, info);
    if (info->frameSamples <= 0 || info->framesInSuperframe <= 0 || info->channels <= 0
        || info->superframeSize <= 0 || info->samplingRate <= 0) {
        LOG_ERROR("Invalid Atrac9 codec info.");
        return;
    }

    result.resize(info->frameSamples * info->channels * sizeof(uint16_t));

    superframe_frame_idx = 0;
    superframe_data_left = info->superframeSize;
    initialized = true;
}

Atrac9DecoderState::~Atrac9DecoderState() {
    if (decoder_handle) {
        Atrac9ReleaseHandle(decoder_handle);
    }
    delete static_cast<Atrac9CodecInfo *>(atrac9_info);
    context = nullptr;
}
