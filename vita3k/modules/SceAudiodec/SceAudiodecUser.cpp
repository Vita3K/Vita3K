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

#include "SceAudiodecUser.h"

#include <codec/state.h>
#include <util/lock_and_find.h>

enum {
    SCE_AUDIODEC_MP3_ERROR_INVALID_MPEG_VERSION = 0x807F2801
};

enum {
    SCE_AUDIODEC_MP3_MPEG_VERSION_2_5,
    SCE_AUDIODEC_MP3_MPEG_VERSION_RESERVED,
    SCE_AUDIODEC_MP3_MPEG_VERSION_2,
    SCE_AUDIODEC_MP3_MPEG_VERSION_1,
};

typedef std::shared_ptr<DecoderState> DecoderPtr;
typedef std::map<SceUID, DecoderPtr> DecoderStates;
typedef std::set<SceUID> CodecDecoders;
typedef std::map<SceAudiodecCodec, CodecDecoders> CodecDecodersMap;

struct AudiodecState {
    std::mutex mutex;
    DecoderStates decoders;
    CodecDecodersMap codecs;
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
    uint32_t sample_rate;
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

constexpr uint32_t SCE_AUDIODEC_AT9_MAX_ES_SIZE = 1024;
constexpr uint32_t SCE_AUDIODEC_MP3_MAX_ES_SIZE = 1441;
// max size is 1792 for AAC ES if adts is enabled
constexpr uint32_t SCE_AUDIODEC_AAC_MAX_ES_SIZE = 1536;
constexpr uint32_t SCE_AUDIODEC_CELP_MAX_ES_SIZE = 27;

// this value is multiplied by 2 if sbr is enabled
constexpr uint32_t SCE_AUDIODEC_AAC_MAX_PCM_SIZE = KB(2);
constexpr uint32_t SCE_AUDIODEC_MP3_V1_MAX_PCM_SIZE = 2304;
constexpr uint32_t SCE_AUDIODEC_MP3_V2_MAX_PCM_SIZE = 1152;

LIBRARY_INIT_IMPL(SceAudiodec) {
    host.kernel.obj_store.create<AudiodecState>();
}
LIBRARY_INIT_REGISTER(SceAudiodec)

EXPORT(int, sceAudiodecClearContext) {
    return UNIMPLEMENTED();
}

static int create_decoder(HostState &host, SceAudiodecCtrl *ctrl, SceAudiodecCodec codec) {
    const auto state = host.kernel.obj_store.get<AudiodecState>();
    std::lock_guard<std::mutex> lock(state->mutex);

    SceUID handle = host.kernel.get_next_uid();
    ctrl->handle = handle;
    state->codecs[codec].insert(handle);

    switch (codec) {
    case SCE_AUDIODEC_TYPE_AT9: {
        SceAudiodecInfoAt9 &info = ctrl->info.get(host.mem)->at9;
        DecoderPtr decoder = std::make_shared<Atrac9DecoderState>(info.config_data);
        state->decoders[handle] = decoder;

        ctrl->es_size_max = SCE_AUDIODEC_AT9_MAX_ES_SIZE;
        ctrl->pcm_size_max = decoder->get(DecoderQuery::AT9_SAMPLE_PER_SUPERFRAME)
            * decoder->get(DecoderQuery::CHANNELS) * sizeof(int16_t);
        info.channels = decoder->get(DecoderQuery::CHANNELS);
        info.bit_rate = decoder->get(DecoderQuery::BIT_RATE);
        info.sample_rate = decoder->get(DecoderQuery::SAMPLE_RATE);
        info.super_frame_size = decoder->get(DecoderQuery::AT9_SUPERFRAME_SIZE);
        info.frames_in_super_frame = decoder->get(DecoderQuery::AT9_FRAMES_IN_SUPERFRAME);
        return host.cfg.current_config.disable_at9_decoder ? -1 : 0;
    }
    case SCE_AUDIODEC_TYPE_AAC: {
        SceAudiodecInfoAac &info = ctrl->info.get(host.mem)->aac;
        DecoderPtr decoder = std::make_shared<AacDecoderState>(info.sample_rate, info.channels);
        state->decoders[handle] = decoder;

        ctrl->es_size_max = SCE_AUDIODEC_AAC_MAX_ES_SIZE;
        if (info.is_adts)
            ctrl->es_size_max += 0x100;
        ctrl->pcm_size_max = info.channels * SCE_AUDIODEC_AAC_MAX_PCM_SIZE;
        if (info.is_sbr)
            ctrl->pcm_size_max *= 2;

        LOG_WARN_IF(info.is_adts || info.is_sbr, "report it to dev, is_adts: {}, is_sbr: {}", info.is_adts, info.is_sbr);

        return 0;
    }
    case SCE_AUDIODEC_TYPE_MP3: {
        SceAudiodecInfoMp3 &info = ctrl->info.get(host.mem)->mp3;
        DecoderPtr decoder = std::make_shared<Mp3DecoderState>(info.channels);
        state->decoders[handle] = decoder;

        ctrl->es_size_max = SCE_AUDIODEC_MP3_MAX_ES_SIZE;

        switch (info.version) {
        case SCE_AUDIODEC_MP3_MPEG_VERSION_1:
            ctrl->pcm_size_max = info.channels * SCE_AUDIODEC_MP3_V1_MAX_PCM_SIZE;
            return 0;
        case SCE_AUDIODEC_MP3_MPEG_VERSION_2:
        case SCE_AUDIODEC_MP3_MPEG_VERSION_2_5:
            ctrl->pcm_size_max = info.channels * SCE_AUDIODEC_MP3_V2_MAX_PCM_SIZE;
            return 0;
        default:
            LOG_ERROR("Invalid MPEG version {}.", log_hex(info.version));
            return SCE_AUDIODEC_MP3_ERROR_INVALID_MPEG_VERSION;
        }
    }
    default: {
        LOG_ERROR("Unimplemented audio decoder {}.", codec);
        return -1;
    }
    }
}

EXPORT(int, sceAudiodecCreateDecoder, SceAudiodecCtrl *ctrl, SceAudiodecCodec codec) {
    return create_decoder(host, ctrl, codec);
}

EXPORT(int, sceAudiodecCreateDecoderExternal, SceAudiodecCtrl *ctrl, SceAudiodecCodec codec, void *context, uint32_t size) {
    // I think context is supposed to be just extra memory where I can allocate my context.
    // I'm just going to allocate like regular sceAudiodecCreateDecoder and see how it goes.
    // Almost sure zang has already tried this so :/ - desgroup
    return create_decoder(host, ctrl, codec);
}

EXPORT(int, sceAudiodecCreateDecoderResident) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAudiodecDecode, SceAudiodecCtrl *ctrl) {
    const auto state = host.kernel.obj_store.get<AudiodecState>();
    const DecoderPtr &decoder = lock_and_find(ctrl->handle, state->decoders, state->mutex);

    DecoderSize size = {};

    decoder->send(ctrl->es_data.get(host.mem), ctrl->es_size_max);
    decoder->receive(ctrl->pcm_data.get(host.mem), &size);

    ctrl->es_size_used = std::min(decoder->get_es_size(), ctrl->es_size_max);
    ctrl->pcm_size_given = size.samples * decoder->get(DecoderQuery::CHANNELS) * sizeof(int16_t);
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
    const auto state = host.kernel.obj_store.get<AudiodecState>();
    std::lock_guard<std::mutex> lock(state->mutex);
    state->decoders.erase(ctrl->handle);

    // there are at most 4 different codecs, we can afford to look
    // at all of them (the handle is in one of them)
    for (auto &codec : state->codecs) {
        codec.second.erase(ctrl->handle);
    }

    return 0;
}

EXPORT(int, sceAudiodecDeleteDecoderExternal, SceAudiodecCtrl *ctrl, void *context) {
    return CALL_EXPORT(sceAudiodecDeleteDecoder, ctrl);
}

EXPORT(int, sceAudiodecDeleteDecoderResident) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceAudiodecGetContextSize) {
    STUBBED("fake size");
    return 53;
}

EXPORT(int, sceAudiodecGetInternalError) {
    return UNIMPLEMENTED();
}

EXPORT(SceInt32, sceAudiodecInitLibrary, SceAudiodecCodec codecType, SceAudiodecInitParam *pInitParam) {
    const auto state = host.kernel.obj_store.get<AudiodecState>();
    std::lock_guard<std::mutex> lock(state->mutex);

    state->codecs[codecType] = CodecDecoders();
    return 0;
}

EXPORT(int, sceAudiodecPartlyDecode) {
    return UNIMPLEMENTED();
}

EXPORT(SceInt32, sceAudiodecTermLibrary, SceAudiodecCodec codecType) {
    const auto state = host.kernel.obj_store.get<AudiodecState>();
    std::lock_guard<std::mutex> lock(state->mutex);

    // remove decoders associated with codecType
    for (auto &handle : state->codecs[codecType]) {
        state->decoders.erase(handle);
    }
    state->codecs.erase(codecType);
    return 0;
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
