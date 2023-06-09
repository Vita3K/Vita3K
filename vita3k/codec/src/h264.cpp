// Vita3K emulator project
// Copyright (C) 2023 Vita3K team
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

#include <util/log.h>

extern "C" {
#include <libavcodec/avcodec.h>
}

#include <cassert>

void copy_yuv_data_from_frame(AVFrame *frame, uint8_t *dest, const uint32_t width, const uint32_t height) {
    for (uint32_t a = 0; a < height; a++) {
        memcpy(dest, &frame->data[0][frame->linesize[0] * a], width);
        dest += width;
    }
    for (uint32_t a = 0; a < height / 2; a++) {
        memcpy(dest, &frame->data[1][frame->linesize[1] * a], width / 2);
        dest += width / 2;
    }
    for (uint32_t a = 0; a < height / 2; a++) {
        memcpy(dest, &frame->data[2][frame->linesize[2] * a], width / 2);
        dest += width / 2;
    }
}

uint32_t H264DecoderState::buffer_size(DecoderSize size) {
    return size.width * size.height * 3 / 2;
}

uint32_t H264DecoderState::get(DecoderQuery query) {
    switch (query) {
    case DecoderQuery::WIDTH: return context->width;
    case DecoderQuery::HEIGHT: return context->height;
    default: return 0;
    }
}

bool H264DecoderState::send(const uint8_t *data, uint32_t size) {
    int error = 0;

    std::vector<uint8_t> au_frame(size + AV_INPUT_BUFFER_PADDING_SIZE);
    memcpy(au_frame.data(), data, size);

    AVPacket *packet = av_packet_alloc();
    if (!packet) {
        LOG_ERROR("Error allocating H264 packet.");
        return false;
    }
    error = av_parser_parse2(
        parser, // AVCodecParserContext *s,
        context, // AVCodecContext *avctx,
        &packet->data, // uint8_t **poutbuf,
        &packet->size, // int *poutbuf_size,
        au_frame.data(), // const uint8_t *buf,
        size, // int buf_size,
        pts == ~0ull ? AV_NOPTS_VALUE : pts, // int64_t pts,
        dts == ~0ull ? AV_NOPTS_VALUE : dts, // int64_t dts,
        0 // int64_t pos
    );
    if (error < 0) {
        LOG_WARN("Error parsing H264 packet: {}.", codec_error_name(error));
        av_packet_free(&packet);
        return false;
    }

    packet->pts = parser->pts;
    packet->dts = parser->dts;

    error = avcodec_send_packet(context, packet);
    av_packet_free(&packet);
    if (error < 0) {
        LOG_WARN("Error sending H264 packet: {}.", codec_error_name(error));
        return false;
    }

    return true;
}

bool H264DecoderState::receive(uint8_t *data, DecoderSize *size) {
    AVFrame *frame = av_frame_alloc();

    int error = avcodec_receive_frame(context, frame);
    if (error < 0) {
        LOG_WARN("Error receiving H264 frame: {}.", codec_error_name(error));
        av_frame_free(&frame);
        return false;
    }

    if (data) {
        copy_yuv_data_from_frame(frame, data, width_in, height_in);
    }

    if (size) {
        *size = { { static_cast<uint32_t>(context->width), static_cast<uint32_t>(context->height) } };
    }

    width_out = frame->width;
    height_out = frame->height;

    pts_out = frame->pts;

    av_frame_free(&frame);
    return true;
}

void H264DecoderState::configure(void *options) {
    auto *opt = reinterpret_cast<H264DecoderOptions *>(options);

    pts = static_cast<uint64_t>(opt->pts_upper) << 32u | static_cast<uint64_t>(opt->pts_lower);
    dts = static_cast<uint64_t>(opt->dts_upper) << 32u | static_cast<uint64_t>(opt->dts_lower);
}

void H264DecoderState::set_res(const uint32_t width, const uint32_t height) {
    width_in = width;
    height_in = height;
}

void H264DecoderState::get_res(uint32_t &width, uint32_t &height) {
    width = width_out;
    height = height_out;
}

void H264DecoderState::get_pts(uint32_t &upper, uint32_t &lower) {
    upper = pts_out >> 32u;
    lower = pts_out & 0xFFFFFFFF;
}

H264DecoderState::H264DecoderState(uint32_t width, uint32_t height) {
    const AVCodec *codec = avcodec_find_decoder(AV_CODEC_ID_H264);
    assert(codec);

    parser = av_parser_init(codec->id);
    assert(parser);
    parser->flags |= PARSER_FLAG_COMPLETE_FRAMES;

    context = avcodec_alloc_context3(codec);
    assert(context);
    context->width = width;
    context->height = height;

    int result = avcodec_open2(context, codec, nullptr);
    assert(result == 0);
}

H264DecoderState::~H264DecoderState() {
    av_parser_close(parser);
}
