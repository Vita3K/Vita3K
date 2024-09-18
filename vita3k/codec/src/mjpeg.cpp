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

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavcodec/bsf.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}

#include <util/align.h>
#include <util/log.h>

#include <cassert>

namespace {

struct JpegInfo {
    int mcu_width;
    int mcu_height;
    int width;
    int height;
};

struct ChannelInfo {
    uint32_t width_divisor;
    uint32_t height_divisor;
    uint32_t offset;
};

JpegInfo parse_jpeg_header(const uint8_t *data, uint32_t size) {
    // JPEG marker codes
    const uint8_t MARKER_SOI = 0xD8;
    const uint8_t MARKER_EOI = 0xD9;
    const uint8_t MARKER_SOS = 0xDA;
    const uint8_t MARKER_SOF0 = 0xC0;

    JpegInfo info = { 0 };

    if (size < 2 || data[0] != 0xFF || data[1] != MARKER_SOI) {
        return info;
    }

    uint32_t offset = 2;
    while (offset < size - 1) {
        if (data[offset] != 0xFF) {
            offset++;
            continue;
        }

        uint8_t marker = data[offset + 1];
        offset += 2;

        if (marker == MARKER_EOI || marker == MARKER_SOS) {
            break;
        }

        if (marker == MARKER_SOF0) {
            if (offset + 8 > size)
                break;

            info.height = (data[offset + 3] << 8) | data[offset + 4];
            info.width = (data[offset + 5] << 8) | data[offset + 6];
            int components = data[offset + 7];

            int max_h_factor = 0, max_v_factor = 0;
            for (int i = 0; i < components; i++) {
                int h_factor = (data[offset + 8 + i * 3 + 1] >> 4) & 0xF;
                int v_factor = data[offset + 8 + i * 3 + 1] & 0xF;
                max_h_factor = (h_factor > max_h_factor) ? h_factor : max_h_factor;
                max_v_factor = (v_factor > max_v_factor) ? v_factor : max_v_factor;
            }

            info.mcu_width = max_h_factor * 8;
            info.mcu_height = max_v_factor * 8;

            break;
        }

        if (offset + 2 > size)
            break;
        uint16_t segment_length = (data[offset] << 8) | data[offset + 1];
        offset += segment_length;
    }

    return info;
}

} // anonymous namespace

void convert_yuv_to_rgb(const uint8_t *yuv, uint8_t *rgba, uint32_t width, uint32_t height, const DecoderColorSpace color_space) {
    AVPixelFormat format = AV_PIX_FMT_YUVJ444P;
    int strides_divisor = 1;
    int slice_position = 8;

    switch (color_space) {
    case COLORSPACE_YUV444P:
        format = AV_PIX_FMT_YUV444P;
        strides_divisor = 1;
        slice_position = 8; // 2
        break;
    case COLORSPACE_YUV422P:
        format = AV_PIX_FMT_YUV422P;
        strides_divisor = 2;
        slice_position = 6; // 1.5
        break;
    case COLORSPACE_YUV420P:
        format = AV_PIX_FMT_YUV420P;
        strides_divisor = 2;
        slice_position = 5; // 1.25
        break;
    }

    SwsContext *context = sws_getContext(width, height, format, width, height, AV_PIX_FMT_RGBA, SWS_FULL_CHR_H_INT | SWS_ACCURATE_RND, nullptr, nullptr, nullptr);
    assert(context);

    const uint8_t *slices[] = {
        &yuv[0], // Y Slice
        &yuv[width * height], // U Slice
        &yuv[width * height * slice_position / 4], // V Slice
    };

    int strides[] = {
        static_cast<int>(width),
        static_cast<int>(width) / strides_divisor,
        static_cast<int>(width) / strides_divisor,
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

void convert_rgb_to_yuv(const uint8_t *rgba, uint8_t *yuv, uint32_t width, uint32_t height, const DecoderColorSpace color_space, int32_t inPitch) {
    AVPixelFormat format = AV_PIX_FMT_NONE;
    int strides_divisor = 1;
    int slice_position = 8;

    switch (color_space) {
    case COLORSPACE_YUV444P:
        format = AV_PIX_FMT_YUV444P;
        strides_divisor = 1;
        slice_position = 8; // 2
        break;
    case COLORSPACE_YUV422P:
        format = AV_PIX_FMT_YUV422P;
        strides_divisor = 2;
        slice_position = 6; // 1.5
        break;
    case COLORSPACE_YUV420P:
        format = AV_PIX_FMT_YUV420P;
        strides_divisor = 2;
        slice_position = 5; // 1.25
        break;
    }

    SwsContext *context = sws_getContext(width, height, AV_PIX_FMT_RGBA, width, height, format,
        SWS_FULL_CHR_H_INT | SWS_ACCURATE_RND, nullptr, nullptr, nullptr);
    assert(context);

    const uint8_t *slices[] = {
        rgba,
    };

    int strides[] = {
        inPitch * 4,
    };

    uint8_t *dst_slices[] = {
        &yuv[0], // Y Slice
        &yuv[width * height], // U Slice
        &yuv[static_cast<uint32_t>(width * height * slice_position / 4)], // V Slice
    };

    const int dst_strides[] = {
        static_cast<int>(width),
        static_cast<int>(width) / strides_divisor,
        static_cast<int>(width) / strides_divisor,
    };

    int error = sws_scale(context, slices, strides, 0, height, dst_slices, dst_strides);
    sws_freeContext(context);
    assert(error == height);
}

int convert_yuv_to_jpeg(const uint8_t *yuv, uint8_t *jpeg, uint32_t width, uint32_t height, uint32_t max_size, const DecoderColorSpace color_space, int32_t compress_ratio) {
    AVPixelFormat format = AV_PIX_FMT_YUV444P;

    const AVCodec *codec = avcodec_find_encoder(AV_CODEC_ID_MJPEG);
    assert(codec);

    AVCodecContext *context = avcodec_alloc_context3(codec);
    assert(context);

    switch (color_space) {
    case COLORSPACE_YUV444P:
        format = AV_PIX_FMT_YUVJ444P;
        break;
    case COLORSPACE_YUV422P:
        format = AV_PIX_FMT_YUVJ422P;
        break;
    case COLORSPACE_YUV420P:
        format = AV_PIX_FMT_YUVJ420P;
        break;
    }

    context->width = width;
    context->height = height;
    context->pix_fmt = format;
    context->time_base.num = 1;
    context->time_base.den = 25;
    context->flags |= AV_CODEC_FLAG_QSCALE;

    context->qmin = 1;

    int ret = avcodec_open2(context, codec, NULL);
    assert(ret >= 0);

    AVFrame *frame = av_frame_alloc();
    assert(frame);

    frame->format = context->pix_fmt;
    frame->width = context->width;
    frame->height = context->height;
    frame->quality = FF_QP2LAMBDA * ((compress_ratio) * (16 - 1) / 255 + 1);

    ret = av_image_fill_arrays(frame->data, frame->linesize, yuv, context->pix_fmt, context->width, context->height, 1);
    assert(ret >= 0);

    AVPacket *pkt = av_packet_alloc();

    ret = avcodec_send_frame(context, frame);
    assert(ret >= 0);

    ret = avcodec_receive_packet(context, pkt);
    if (ret < 0) {
        av_packet_unref(pkt);
        av_frame_free(&frame);
        avcodec_free_context(&context);
        return ret;
    }

    const AVBitStreamFilter *bsf = av_bsf_get_by_name("mjpeg2jpeg");

    AVBSFContext *bsf_ctx = NULL;
    ret = av_bsf_alloc(bsf, &bsf_ctx);
    if (ret < 0) {
        av_packet_unref(pkt);
        av_frame_free(&frame);
        avcodec_free_context(&context);
        return ret;
    }

    bsf_ctx->par_in->codec_id = context->codec_id;
    ret = av_bsf_init(bsf_ctx);
    if (ret < 0) {
        av_bsf_free(&bsf_ctx);
        av_packet_unref(pkt);
        av_frame_free(&frame);
        avcodec_free_context(&context);
        return ret;
    }

    ret = av_bsf_send_packet(bsf_ctx, pkt);
    if (ret < 0) {
        av_bsf_free(&bsf_ctx);
        av_packet_unref(pkt);
        av_frame_free(&frame);
        avcodec_free_context(&context);
        return ret;
    }

    ret = av_bsf_receive_packet(bsf_ctx, pkt);

    uint32_t size = pkt->size;
    if (ret == 0 && size <= max_size) {
        memcpy(jpeg, pkt->data, pkt->size);
    }

    av_bsf_free(&bsf_ctx);
    av_packet_unref(pkt);
    av_frame_free(&frame);
    avcodec_free_context(&context);
    if (size > max_size || ret < 0) {
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

    // Parse JPEG header to get MCU size
    JpegInfo jpeg_info = parse_jpeg_header(data, size);
    this->mcu_height = jpeg_info.mcu_height;
    this->mcu_width = jpeg_info.mcu_width;

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

    if (mcu_width == 0 || mcu_height == 0) {
        LOG_WARN_ONCE("Mjpeg MCU size is not set. Falling back to 8x8.");
        mcu_width = mcu_height = 8;
    }

    // Calculate MCU-aligned dimensions
    uint32_t aligned_width = align(frame->width, mcu_width);
    uint32_t aligned_height = align(frame->height, mcu_height);

    if (frame->width != aligned_width || frame->height != aligned_height) {
        LOG_INFO_ONCE("Mjpeg frame dimensions are not MCU-aligned. MCU size: {}x{}, frame size: {}x{} -> aligned size: {}x{}.",
            mcu_width, mcu_height, frame->width, frame->height, aligned_width, aligned_height);
    }

    if (data) {
        ChannelInfo channel_info[3];
        uint32_t total_pixel_count = aligned_width * aligned_height;

        switch (frame->format) {
        case AV_PIX_FMT_YUVJ444P:
            for (int i = 0; i < 3; i++) {
                channel_info[i] = { 1, 1, total_pixel_count * i };
            }
            this->color_space_out = COLORSPACE_YUV444P;
            break;
        case AV_PIX_FMT_YUVJ422P:
            channel_info[0] = { 1, 1, 0 };
            channel_info[1] = { 2, 1, total_pixel_count };
            channel_info[2] = { 2, 1, total_pixel_count * 3 / 2 };
            this->color_space_out = COLORSPACE_YUV422P;
            break;
        case AV_PIX_FMT_YUVJ420P:
            channel_info[0] = { 1, 1, 0 };
            channel_info[1] = { 2, 2, total_pixel_count };
            channel_info[2] = { 2, 2, total_pixel_count * 5 / 4 };
            this->color_space_out = COLORSPACE_YUV420P;
            break;
        default:
            LOG_WARN("Mjpeg frame is in unimplemented format {}.", frame->format);
            av_frame_free(&frame);
            return false;
        }

        for (int a = 0; a < 3; a++) {
            uint32_t component_width = aligned_width / channel_info[a].width_divisor;
            uint32_t component_height = aligned_height / channel_info[a].height_divisor;
            uint32_t frame_component_width = frame->width / channel_info[a].width_divisor;
            uint32_t frame_component_height = frame->height / channel_info[a].height_divisor;

            for (uint32_t y = 0; y < component_height; y++) {
                uint8_t *dst = &data[channel_info[a].offset + y * component_width];
                const uint8_t *src = frame->data[a] + (y < frame_component_height ? y * frame->linesize[a] : (frame_component_height - 1) * frame->linesize[a]);

                if (y < frame_component_height) {
                    std::memcpy(dst, src, frame_component_width);
                    // Pad the rest with the last pixel value
                    std::memset(dst + frame_component_width, dst[frame_component_width - 1], component_width - frame_component_width);
                } else {
                    // Copy the last row for padding
                    std::memcpy(dst, dst - component_width, component_width);
                }
            }
        }
    }

    if (size) {
        size->width = frame->width;
        size->height = frame->height;
        size->pitch_width = aligned_width;
        size->pitch_height = aligned_height;
    }

    av_frame_free(&frame);

    return true;
}

DecoderColorSpace MjpegDecoderState::get_color_space() {
    return this->color_space_out;
}

MjpegDecoderState::MjpegDecoderState() {
    const AVCodec *codec = avcodec_find_decoder(AV_CODEC_ID_MJPEG);
    assert(codec);

    context = avcodec_alloc_context3(codec);
    assert(context);
    int error = avcodec_open2(context, codec, nullptr);
    assert(error == 0);
    this->color_space_out = COLORSPACE_UNKNOWN;
}
