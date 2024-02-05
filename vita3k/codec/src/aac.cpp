// Vita3K emulator project
// Copyright (C) 2024 Vita3K team
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

#include <map>

#define DEBUG

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/opt.h>
#include <libswresample/swresample.h>

#include <libavcodec/codec_internal.h>
}

#include <util/log.h>

AacDecoderState::AacDecoderState(uint32_t sample_rate, uint32_t channels) {
    codec = avcodec_find_decoder(AV_CODEC_ID_AAC);
    assert(codec);

    context = avcodec_alloc_context3(codec);
    assert(context);

    frame = av_frame_alloc();

    context->codec_type = AVMEDIA_TYPE_AUDIO;
    av_channel_layout_default(&context->ch_layout, channels);
    context->sample_rate = sample_rate;

    int err = avcodec_open2(context, codec, nullptr);
    assert(err == 0);

    swr = nullptr;
    int ret = swr_alloc_set_opts2(&swr,
        &context->ch_layout, AV_SAMPLE_FMT_S16, sample_rate,
        &context->ch_layout, AV_SAMPLE_FMT_FLTP, sample_rate,
        0, nullptr);
    assert(ret == 0);

    ret = swr_init(swr);
    assert(ret == 0);
}

AacDecoderState::~AacDecoderState() {
    av_frame_free(&frame);
    swr_free(&swr);
}

uint32_t AacDecoderState::get(DecoderQuery query) {
    switch (query) {
    case DecoderQuery::CHANNELS: return context->ch_layout.nb_channels;
    case DecoderQuery::BIT_RATE: return context->bit_rate;
    case DecoderQuery::SAMPLE_RATE: return context->sample_rate;
    default:
        return 0;
    }
}

bool AacDecoderState::send(const uint8_t *data, uint32_t size) {
    AVPacket *packet = av_packet_alloc();
    packet->data = const_cast<uint8_t *>(data);
    packet->size = size;

    av_frame_unref(frame);

    const FFCodec *ff_codec = ffcodec(codec);
    int got_frame;
    int len = ff_codec->cb.decode(context, frame, &got_frame, packet);
    assert(got_frame);

    av_packet_free(&packet);
    if (len < 0) {
        LOG_WARN("Error sending Aac packet: {}.", codec_error_name(len));
        return false;
    }

    es_size_used = static_cast<uint32_t>(len);

    return true;
}

bool AacDecoderState::receive(uint8_t *data, DecoderSize *size) {
    assert(frame->format == AV_SAMPLE_FMT_FLTP);

    if (data) {
        int ret = swr_convert(swr, &data, frame->nb_samples, const_cast<const uint8_t **>(frame->extended_data), frame->nb_samples);
        assert(ret >= 0);
    }

    if (size) {
        size->samples = frame->nb_samples;
    }

    return true;
}

uint32_t AacDecoderState::get_es_size() {
    return es_size_used;
}
