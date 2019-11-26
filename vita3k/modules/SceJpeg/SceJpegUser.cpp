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

#include "SceJpegUser.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
}

#include <climits>

namespace emu {
    struct SceJpegMJpegInitInfo {
        uint32_t size;
        int32_t decoder_count;
        int32_t options;
    };

    struct SceJpegPitch {
        uint32_t x;
        uint32_t y;
    };

    struct SceJpegOutputInfo {
        int32_t color_space;
        uint16_t width;
        uint16_t height;
        uint32_t output_size;
        uint32_t unknown1;
        uint32_t unknown2;
        SceJpegPitch pitch[4];
    };
}

void init(MJpegState &state) {
    AVCodec *codec = avcodec_find_decoder(AV_CODEC_ID_MJPEG);
    assert(codec);

    state.decoder = avcodec_alloc_context3(codec);
    assert(state.decoder);
    int error = avcodec_open2(state.decoder, codec, nullptr);
    assert(error == 0);

    state.initialized = true;
}

void close(MJpegState &state) {
    avcodec_close(state.decoder);
    avcodec_free_context(&state.decoder);

    state.initialized = false;
}

AVFrame *decode(MJpegState &state, const uint8_t *data, uint32_t size) {
    std::vector<uint8_t> jpeg_buffer(size + AV_INPUT_BUFFER_PADDING_SIZE);
    std::memcpy(jpeg_buffer.data(), data, size);

    int error;

    AVPacket *packet = av_packet_alloc();
    packet->data = jpeg_buffer.data();
    packet->size = size;
    error = avcodec_send_packet(state.decoder, packet);
    assert(error == 0);
    av_packet_free(&packet);

    AVFrame *frame = av_frame_alloc();
    error = avcodec_receive_frame(state.decoder, frame);
    assert(error == 0);

    return frame;
}

EXPORT(int, sceJpegCreateSplitDecoder) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceJpegCsc) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceJpegDecodeMJpeg) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceJpegDecodeMJpegYCbCr, const uint8_t *jpeg_data, uint32_t jpeg_size,
    uint8_t *output, uint32_t output_size, int mode, void *buffer, uint32_t buffer_size) {
    MJpegState &state = host.kernel.mjpeg_state;

    AVFrame *frame = decode(state, jpeg_data, jpeg_size);

    uint8_t *channels[] = {
        &output[0], // y
        &output[frame->width * frame->height], // u
        &output[frame->width * frame->height * 2], // v
    };

    assert(frame->format == AV_PIX_FMT_YUVJ444P);

    // Copy YUV444 data.
    for (uint32_t a = 0; a < 3; a++) {
        for (uint32_t b = 0; b < frame->height; b++) {
            std::memcpy(&channels[a][b * frame->width], &frame->data[a][b * frame->linesize[a]], frame->width);
        }
    }

    // Top 16 bits = width, bottom 16 bits = height.
    return (frame->width << 16u) | frame->height;
}

EXPORT(int, sceJpegDeleteSplitDecoder) {
    return UNIMPLEMENTED();
}

EXPORT(int, sceJpegFinishMJpeg) {
    close(host.kernel.mjpeg_state);

    return 0;
}

EXPORT(int, sceJpegGetOutputInfo, const uint8_t *jpeg_data, uint32_t jpeg_size,
    int32_t format, int32_t mode, emu::SceJpegOutputInfo *output) {
    MJpegState &state = host.kernel.mjpeg_state;

    AVFrame *frame = decode(state, jpeg_data, jpeg_size);

    output->width = frame->width;
    output->height = frame->height;
    output->color_space = 2u << 16u;
    output->output_size = frame->width * frame->height * 3;

    av_frame_free(&frame);

    return 0;
}

// TODO: Decoder options are ignored for the time being.
EXPORT(int, sceJpegInitMJpeg, int32_t decoder_count) {
    init(host.kernel.mjpeg_state);

    return 0;
}

EXPORT(int, sceJpegInitMJpegWithParam, const emu::SceJpegMJpegInitInfo *info) {
    init(host.kernel.mjpeg_state);

    return 0;
}

EXPORT(int, sceJpegMJpegCsc, uint8_t *rgba, const uint8_t *yuv,
    int32_t size, int32_t image_width, int32_t format, int32_t sampling) {
    uint32_t width = size >> 16u;
    uint32_t height = size & (~0u >> 16u);

    SwsContext *context = sws_getContext(width, height, AV_PIX_FMT_YUV444P, width, height, AV_PIX_FMT_RGBA,
        0, nullptr, nullptr, nullptr);
    assert(context);

    const uint8_t *slices[] = {
        &yuv[0], // Y Slice
        &yuv[width * height], // U Slice
        &yuv[width * height * 2], // V Slice
    };

    int strides[] = {
        static_cast<int>(width),
        static_cast<int>(width),
        static_cast<int>(width),
    };

    uint8_t *dst_slices[] = {
        rgba,
    };

    const int dst_strides[] = {
        static_cast<int>(width * 4),
    };

    int error = sws_scale(context, slices, strides, 0, height, dst_slices, dst_strides);
    assert(error == height);
    sws_freeContext(context);

    return 0;
}

EXPORT(int, sceJpegSplitDecodeMJpeg) {
    return UNIMPLEMENTED();
}

BRIDGE_IMPL(sceJpegCreateSplitDecoder)
BRIDGE_IMPL(sceJpegCsc)
BRIDGE_IMPL(sceJpegDecodeMJpeg)
BRIDGE_IMPL(sceJpegDecodeMJpegYCbCr)
BRIDGE_IMPL(sceJpegDeleteSplitDecoder)
BRIDGE_IMPL(sceJpegFinishMJpeg)
BRIDGE_IMPL(sceJpegGetOutputInfo)
BRIDGE_IMPL(sceJpegInitMJpeg)
BRIDGE_IMPL(sceJpegInitMJpegWithParam)
BRIDGE_IMPL(sceJpegMJpegCsc)
BRIDGE_IMPL(sceJpegSplitDecodeMJpeg)
