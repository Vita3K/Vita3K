// Vita3K emulator project
// Copyright (C) 2018 Vita3K team
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

#include "SceAudiodecUser.h"

#include <util/lock_and_find.h>

enum SceAudiodecCodec : uint32_t {
    SCE_AUDIODEC_TYPE_AT9 = 0x1003,
    SCE_AUDIODEC_TYPE_MP3 = 0x1004,
    SCE_AUDIODEC_TYPE_AAC = 0x1005,
    SCE_AUDIODEC_TYPE_CELP = 0x1006,
};

struct SceAudiodecInfoAt9 {
    uint32_t config_data;
    uint32_t channels;
    uint32_t bit_rate;
    uint32_t sample_rate;
    uint32_t super_frame_size;
    uint32_t frames_in_super_frame;
};

struct SceAudiodecInfoMp3 {
    uint32_t channels;
    uint32_t version;
};

struct SceAudiodecInfoAac {
    uint32_t is_adts;
    uint32_t channels;
    uint32_t sample_ate;
    uint32_t is_sbr;
};

struct SceAudiodecInfoCelp {
    uint32_t excitation_mode;
    uint32_t sample_rate;
    uint32_t bit_rate;
    uint32_t lost_count;
};

struct SceAudiodecInfo {
    uint32_t size;
    union {
        SceAudiodecInfoAt9 at9;
        SceAudiodecInfoMp3 mp3;
        SceAudiodecInfoAac aac;
        SceAudiodecInfoCelp celp;
    };
};

struct SceAudiodecCtrl {
    uint32_t size;
    SceUID handle;
    Ptr<uint8_t> es_data;
    uint32_t es_size_used;
    uint32_t es_size_max;
    Ptr<uint8_t> pcm_data;
    uint32_t pcm_size_given;
    uint32_t pcm_size_max;
    uint32_t word_length;
    Ptr<SceAudiodecInfo> info;
};

EXPORT(int, sceAudiodecClearContext) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAudiodecCreateDecoder, SceAudiodecCtrl *ctrl, SceAudiodecCodec codec) {
    std::lock_guard<std::mutex> lock(host.kernel.mutex);

    SceUID handle = host.kernel.get_next_uid();
    ctrl->handle = handle;

    switch (codec) {
        case SCE_AUDIODEC_TYPE_AT9: {
            SceAudiodecInfoAt9 &info = ctrl->info.get(host.mem)->at9;
            DecoderPtr decoder = std::make_shared<Atrac9DecoderState>(info.config_data);
            host.kernel.decoders[handle] = decoder;


            uint32_t block_align = decoder->get(DecoderQuery::AT9_BLOCK_ALIGN);
            uint32_t sps = decoder->get(DecoderQuery::AT9_SAMPLE_PER_SUPERFRAME);
            uint32_t fis = decoder->get(DecoderQuery::AT9_FRAMES_IN_SUPERFRAME);
            uint32_t ss = decoder->get(DecoderQuery::AT9_SUPERFRAME_SIZE);

            ctrl->es_size_max = decoder->get(DecoderQuery::AT9_SUPERFRAME_SIZE);
            ctrl->pcm_size_max = decoder->get(DecoderQuery::AT9_SAMPLE_PER_SUPERFRAME)
                * decoder->get(DecoderQuery::CHANNELS) * sizeof(int16_t);
            info.channels = decoder->get(DecoderQuery::CHANNELS);
            info.bit_rate = decoder->get(DecoderQuery::BIT_RATE);
            info.sample_rate = decoder->get(DecoderQuery::SAMPLE_RATE);
            info.super_frame_size = decoder->get(DecoderQuery::AT9_SUPERFRAME_SIZE);
            info.frames_in_super_frame = decoder->get(DecoderQuery::AT9_FRAMES_IN_SUPERFRAME);
            return 0;
        }
        default: {
            LOG_ERROR("Unimplemented audio decoder {}.", codec);
            return -1;
        }
    }
}

EXPORT(int, sceAudiodecCreateDecoderExternal) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAudiodecCreateDecoderResident) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAudiodecDecode, SceAudiodecCtrl *ctrl) {
    const DecoderPtr &decoder = lock_and_find(ctrl->handle, host.kernel.decoders, host.kernel.mutex);

    DecoderSize size = { };

    decoder->send(ctrl->es_data.get(host.mem), ctrl->es_size_max);
    decoder->receive(ctrl->pcm_data.get(host.mem), &size);

    ctrl->es_size_used = ctrl->es_size_max;
    ctrl->pcm_size_given = decoder->get(DecoderQuery::AT9_SAMPLE_PER_SUPERFRAME)
        * decoder->get(DecoderQuery::CHANNELS) * sizeof(int16_t);
    assert(ctrl->es_size_used <= ctrl->es_size_max);
    assert(ctrl->pcm_size_given <= ctrl->pcm_size_max);

    return 0;
}

EXPORT(int, sceAudiodecDecodeNFrames) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAudiodecDecodeNStreams) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAudiodecDeleteDecoder, SceAudiodecCtrl *ctrl) {
    std::lock_guard<std::mutex> lock(host.kernel.mutex);
    host.kernel.decoders.erase(ctrl->handle);

    return 0;
}

EXPORT(int, sceAudiodecDeleteDecoderExternal) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAudiodecDeleteDecoderResident) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAudiodecGetContextSize) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAudiodecGetInternalError) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAudiodecInitLibrary) {
    return STUBBED("EMPTY");
}

EXPORT(int, sceAudiodecPartlyDecode) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAudiodecTermLibrary) {
    return STUBBED("EMPTY");
}

BRIDGE_IMPL(sceAudiodecClearContext)
BRIDGE_IMPL(sceAudiodecCreateDecoder)
BRIDGE_IMPL(sceAudiodecCreateDecoderExternal)
BRIDGE_IMPL(sceAudiodecCreateDecoderResident)
BRIDGE_IMPL(sceAudiodecDecode)
BRIDGE_IMPL(sceAudiodecDecodeNFrames)
BRIDGE_IMPL(sceAudiodecDecodeNStreams)
BRIDGE_IMPL(sceAudiodecDeleteDecoder)
BRIDGE_IMPL(sceAudiodecDeleteDecoderExternal)
BRIDGE_IMPL(sceAudiodecDeleteDecoderResident)
BRIDGE_IMPL(sceAudiodecGetContextSize)
BRIDGE_IMPL(sceAudiodecGetInternalError)
BRIDGE_IMPL(sceAudiodecInitLibrary)
BRIDGE_IMPL(sceAudiodecPartlyDecode)
BRIDGE_IMPL(sceAudiodecTermLibrary)
