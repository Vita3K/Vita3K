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

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}

#include <util/log.h>

#include <cassert>

void convert_yuv_to_rgb(const uint8_t *yuv, uint8_t *rgba, uint32_t width, uint32_t height, const bool is_yuv420) {
    SwsContext *context = sws_getContext(width, height, is_yuv420 ? AV_PIX_FMT_YUV420P : AV_PIX_FMT_YUV444P, width, height, AV_PIX_FMT_RGBA,
        0, nullptr, nullptr, nullptr);
    assert(context);

    const uint8_t *slices[] = {
        &yuv[0], // Y Slice
        &yuv[width * height], // U Slice
        &yuv[static_cast<uint32_t>(width * height * (is_yuv420 ? 1.25 : 2))], // V Slice
    };

    int strides[] = {
        static_cast<int>(width),
        static_cast<int>(width) / (is_yuv420 ? 2 : 1),
        static_cast<int>(width) / (is_yuv420 ? 2 : 1),
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
}

void convert_rgb_to_yuv(const uint8_t *rgba, uint8_t *yuv, uint32_t width, uint32_t height, const bool is_yuv420) {
    SwsContext *context = sws_getContext(width, height, AV_PIX_FMT_RGBA, width, height, is_yuv420 ? AV_PIX_FMT_YUV420P : AV_PIX_FMT_YUV422P,
        0, nullptr, nullptr, nullptr);
    assert(context);

    const uint8_t *slices[] = {
        rgba,
    };

    int strides[] = {
        static_cast<int>(width * 4),
    };

    uint8_t *dst_slices[] = {
        &yuv[0], // Y Slice
        &yuv[width * height], // U Slice
        &yuv[static_cast<uint32_t>(width * height * (is_yuv420 ? 1.25 : 1.5))], // V Slice
    };

    const int dst_strides[] = {
        static_cast<int>(width),
        static_cast<int>(width) / (is_yuv420 ? 2 : 1),
        static_cast<int>(width) / (is_yuv420 ? 2 : 1),
    };

    int error = sws_scale(context, slices, strides, 0, height, dst_slices, dst_strides);
    sws_freeContext(context);
    assert(error == height);
}

void convert_yuv420_to_yuv444(const uint8_t *yuv420, uint8_t *yuv444, uint32_t width, uint32_t height) {
    SwsContext *context = sws_getContext(width, height, AV_PIX_FMT_YUV420P, width, height, AV_PIX_FMT_YUV444P,
        0, nullptr, nullptr, nullptr);
    assert(context);

    const uint8_t *src_slices[] = {
        &yuv420[0], // Y Slice
        &yuv420[width * height], // U Slice
        &yuv420[static_cast<uint32_t>(width * height * 1.25)], // V Slice
    };

    int src_strides[] = {
        static_cast<int>(width),
        static_cast<int>(width) / 2,
        static_cast<int>(width) / 2,
    };

    uint8_t *dst_slices[] = {
        &yuv444[0], // Y Slice
        &yuv444[width * height], // U Slice
        &yuv444[2 * width * height], // V Slice
    };

    const int dst_strides[] = {
        static_cast<int>(width),
        static_cast<int>(width),
        static_cast<int>(width),
    };

    int error = sws_scale(context, src_slices, src_strides, 0, height, dst_slices, dst_strides);
    sws_freeContext(context);
    assert(error == height);
}

int convert_yuv_to_jpeg(const uint8_t *yuv, uint8_t *jpeg, uint32_t width, uint32_t height, uint32_t max_size, bool is_yuv420) {
    const AVCodec *codec = avcodec_find_encoder(AV_CODEC_ID_MJPEG);
    assert(codec);

    AVCodecContext *context = avcodec_alloc_context3(codec);
    assert(context);

    context->width = width;
    context->height = height;
    context->pix_fmt = is_yuv420 ? AV_PIX_FMT_YUVJ420P : AV_PIX_FMT_YUVJ422P;
    context->time_base.num = 1;
    context->time_base.den = 25;

    int ret = avcodec_open2(context, codec, NULL);
    assert(ret >= 0);

    AVFrame *frame = av_frame_alloc();
    assert(frame);

    frame->format = context->pix_fmt;
    frame->width = context->width;
    frame->height = context->height;

    ret = av_image_fill_arrays(frame->data, frame->linesize, yuv, context->pix_fmt, context->width, context->height, 1);
    assert(ret >= 0);

    AVPacket *pkt = av_packet_alloc();

    ret = avcodec_send_frame(context, frame);
    assert(ret >= 0);

    ret = avcodec_receive_packet(context, pkt);
    uint32_t size = pkt->size;
    if (ret == 0 && size <= max_size) {
        memcpy(jpeg, pkt->data, pkt->size);
    }

    av_packet_unref(pkt);
    av_frame_free(&frame);
    avcodec_free_context(&context);
    if (size > max_size) {
        return -1;
    }
    return size;
}

bool MjpegDecoderState::send(const uint8_t *data, uint32_t size) {
    std::vector<uint8_t> jpeg_buffer(size + AV_INPUT_BUFFER_PADDING_SIZE);
    std::memcpy(jpeg_buffer.data(), data, size);

    AVPacket *packet = av_packet_alloc();
    packet->data = jpeg_buffer.data();
    packet->size = size;
    int error = avcodec_send_packet(context, packet);
    av_packet_free(&packet);

    if (error < 0) {
        LOG_WARN("Error sending Mjpeg packet: {}.", codec_error_name(error));
        return false;
    }
    return true;
}

bool MjpegDecoderState::receive(uint8_t *data, DecoderSize *size) {
    AVFrame *frame = av_frame_alloc();
    int error = avcodec_receive_frame(context, frame);
    if (error < 0) {
        LOG_WARN("Error receiving Mjpeg frame: {}.", codec_error_name(error));
        av_frame_free(&frame);
        return false;
    }

    if (data) {
        switch (frame->format) {
        case AV_PIX_FMT_YUVJ444P: {
            uint8_t *channels[] = {
                &data[0], // y
                &data[frame->width * frame->height], // u
                &data[frame->width * frame->height * 2], // v
            };

            // Copy YUV444 data.
            for (uint32_t a = 0; a < 3; a++) {
                for (int b = 0; b < frame->height; b++) {
                    std::memcpy(&channels[a][b * frame->width], &frame->data[a][b * frame->linesize[a]], frame->width);
                }
            }
            break;
        }
        case AV_PIX_FMT_YUVJ420P: {
            uint8_t *channels[] = {
                &data[0], // y
                &data[frame->width * frame->height], // u
                &data[frame->width * frame->height * 5 / 4], // v
            };

            // Copy YUV420 data.
            for (uint32_t a = 0; a < 3; a++) {
                for (int b = 0; b < frame->height / (a ? 2 : 1); b++) {
                    std::memcpy(&channels[a][b * frame->width / (a ? 2 : 1)], &frame->data[a][b * frame->linesize[a]], frame->width / (a ? 2 : 1));
                }
            }
            break;
        }
        default:
            LOG_WARN("Mjpeg frame is in unimplemented format {}.", frame->format);
            av_frame_free(&frame);
            return false;
        }
    }

    if (size) {
        size->width = frame->width;
        size->height = frame->height;
    }

    av_frame_free(&frame);

    return true;
}

MjpegDecoderState::MjpegDecoderState() {
    const AVCodec *codec = avcodec_find_decoder(AV_CODEC_ID_MJPEG);
    assert(codec);

    context = avcodec_alloc_context3(codec);
    assert(context);
    int error = avcodec_open2(context, codec, nullptr);
    assert(error == 0);
}
