// Vita3K emulator project
// Copyright (C) 2025 Vita3K team
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

void convert_yuv_to_rgb(const uint8_t *yuv, uint8_t *rgba, uint32_t frame_width, const DecoderColorSpace color_space, const bool is_bgra, MJpegPitch pitch[4]) {
    AVPixelFormat format = AV_PIX_FMT_YUVJ444P;
    int width = pitch[0].x, height = pitch[0].y;

    switch (color_space) {
    case COLORSPACE_YUV444P:
        format = AV_PIX_FMT_YUV444P;
        break;
    case COLORSPACE_YUV422P:
        format = AV_PIX_FMT_YUV422P;
        break;
    case COLORSPACE_YUV420P:
        format = AV_PIX_FMT_YUV420P;
        break;
    default:
        LOG_WARN("An attempt was made to use an unsupported color space.");
        return;
    }

    SwsContext *context = sws_getContext(width, height, format, width, height, is_bgra ? AV_PIX_FMT_BGRA : AV_PIX_FMT_RGBA, SWS_FULL_CHR_H_INT | SWS_ACCURATE_RND, nullptr, nullptr, nullptr);
    assert(context);

    const uint8_t *slices[] = {
        &yuv[0], // Y Slice
        &yuv[pitch[0].x * pitch[0].y], // U Slice
        &yuv[pitch[0].x * pitch[0].y + pitch[1].x * pitch[1].y], // V Slice
    };

    int strides[] = {
        static_cast<int>(pitch[0].x),
        static_cast<int>(pitch[1].x),
        static_cast<int>(pitch[2].x),
    };

    uint8_t *dst_slices[] = {
        rgba,
    };

    const int dst_strides[] = {
        static_cast<int>(frame_width * 4),
    };

    int error = sws_scale(context, slices, strides, 0, height, dst_slices, dst_strides);
    assert(error == height);
    sws_freeContext(context);
}

void convert_rgb_to_yuv(const uint8_t *rgba, uint8_t *yuv, uint32_t width, uint32_t height, const DecoderColorSpace color_space, int32_t in_pitch) {
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
    default:
        LOG_WARN("An attempt was made to use an unsupported color space.");
        return;
    }

    SwsContext *context = sws_getContext(width, height, AV_PIX_FMT_RGBA, width, height, format,
        SWS_FULL_CHR_H_INT | SWS_ACCURATE_RND, nullptr, nullptr, nullptr);
    assert(context);

    const uint8_t *slices[] = {
        rgba,
    };

    int strides[] = {
        static_cast<int>(in_pitch * 4),
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

static AVPixelFormat colorspace_to_av_pixel_format(DecoderColorSpace color_space) {
    switch (color_space) {
    case COLORSPACE_YUV444P:
        return AV_PIX_FMT_YUVJ444P;
    case COLORSPACE_YUV440P:
        return AV_PIX_FMT_YUVJ440P;
    case COLORSPACE_YUV441P:
        // Do not use AV_PIX_FMT_YUVJ441P, as it is not supported by the mjpeg2jpeg bitstream filter
        LOG_WARN("YUV441 is currently not supported.");
        return AV_PIX_FMT_NONE;
    case COLORSPACE_YUV422P:
        return AV_PIX_FMT_YUVJ422P;
    case COLORSPACE_YUV420P:
        return AV_PIX_FMT_YUVJ420P;
    case COLORSPACE_YUV411P:
        return AV_PIX_FMT_YUVJ411P;
    case COLORSPACE_GRAYSCALE:
        return AV_PIX_FMT_GRAY8;
    default:
        return AV_PIX_FMT_NONE;
    }
}

static DecoderColorSpace av_pixel_format_to_colorspace(AVPixelFormat format) {
    switch (format) {
    case AV_PIX_FMT_YUVJ444P:
        return COLORSPACE_YUV444P;
    case AV_PIX_FMT_YUVJ440P:
        // FFmpeg incorrectly recognizes YUV441P as YUV440P. Please keep this in mind if you encounter any related errors.
        return COLORSPACE_YUV440P;
    case AV_PIX_FMT_YUVJ422P:
        return COLORSPACE_YUV422P;
    case AV_PIX_FMT_YUVJ420P:
        return COLORSPACE_YUV420P;
    case AV_PIX_FMT_YUVJ411P:
        return COLORSPACE_YUV411P;
    case AV_PIX_FMT_GRAY8:
        return COLORSPACE_GRAYSCALE;
    default:
        return COLORSPACE_UNKNOWN;
    }
}

int convert_yuv_to_jpeg(const uint8_t *yuv, uint8_t *jpeg, uint32_t width, uint32_t height, uint32_t max_size, const DecoderColorSpace color_space, int32_t compress_ratio) {
    AVPixelFormat format = AV_PIX_FMT_YUV444P;

    const AVCodec *codec = avcodec_find_encoder(AV_CODEC_ID_MJPEG);
    assert(codec);

    AVCodecContext *context = avcodec_alloc_context3(codec);
    assert(context);

    format = colorspace_to_av_pixel_format(color_space);

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

void calculate_pitch_info(uint32_t width, uint32_t height, int downscale_ratio, DecoderColorSpace color_space, bool use_standard_decoder, MJpegPitch output_pitch[4]) {
    // If the downscale_ratio is 0 (unknown), does not additionally calculate the downscaled_pitch.
    auto calculate_downscaled_pitch = [&](uint32_t w, uint32_t h, int alignX, int alignY) {
        return MJpegPitch{
            align(w / downscale_ratio, alignX),
            align(h / downscale_ratio, alignY)
        };
    };

    MJpegPitch basic_pitch = { width, height };
    int chroma_w_div = 1, chroma_h_div = 1;
    int block_w_size = 1, block_h_size = 1;

    switch (color_space) {
    case COLORSPACE_GRAYSCALE:
        block_w_size = 8;
        block_h_size = 8;
        break;
    case COLORSPACE_YUV411P:
        block_w_size = 32;
        block_h_size = 8;
        chroma_w_div = 4;
        break;
    case COLORSPACE_YUV420P:
        block_w_size = 16;
        block_h_size = 16;
        chroma_w_div = chroma_h_div = 2;
        break;
    case COLORSPACE_YUV422P:
        block_w_size = 16;
        block_h_size = 8;
        chroma_w_div = 2;
        break;
    case COLORSPACE_YUV440P:
        block_w_size = 8;
        block_h_size = 16;
        chroma_h_div = 2;
        break;
    case COLORSPACE_YUV441P:
        block_w_size = 8;
        block_h_size = 32;
        chroma_h_div = 4;
        break;
    case COLORSPACE_YUV444P:
        block_w_size = 8;
        block_h_size = 8;
        break;
    default:
        LOG_WARN("An attempt was made to use an unsupported color space.");
        break;
    }

    if (downscale_ratio) {
        basic_pitch = calculate_downscaled_pitch(align(width, block_w_size), align(height, block_h_size), block_w_size, use_standard_decoder ? 8 : 1);
    }

    output_pitch[0] = basic_pitch;
    output_pitch[1] = output_pitch[2] = output_pitch[3] = { 0, 0 };
    if (color_space != COLORSPACE_GRAYSCALE) {
        output_pitch[1] = output_pitch[2] = { basic_pitch.x / chroma_w_div, basic_pitch.y / chroma_h_div };
    }

    if (use_standard_decoder && (color_space == COLORSPACE_YUV420P || color_space == COLORSPACE_YUV422P)) {
        output_pitch[1].x = output_pitch[2].x = align(output_pitch[1].x, 16);
    }
}

void MjpegDecoderState::configure(void *options) {
    auto *opt = static_cast<MJpegDecoderOptions *>(options);

    use_standard_decoder = opt->use_standard_decoder;
    downscale_ratio = opt->downscale_ratio;
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

    this->color_space_out = av_pixel_format_to_colorspace(static_cast<AVPixelFormat>(frame->format));
    if (this->color_space_out == COLORSPACE_UNKNOWN) {
        LOG_WARN("Mjpeg frame is in unimplemented format {}.", frame->format);
        av_frame_free(&frame);
        return false;
    }

    int original_width = frame->width;
    int original_height = frame->height;

    calculate_pitch_info(frame->width, frame->height, this->downscale_ratio, this->color_space_out, this->use_standard_decoder, this->pitch);

    if (data) {
        MJpegPitch original_pitch[4];
        calculate_pitch_info(original_width, original_height, 1, this->color_space_out, this->use_standard_decoder, original_pitch);

        std::vector<uint8_t> original_buffer;
        uint8_t *original_data;

        if (this->downscale_ratio != 1) {
            int original_size = 0;
            for (int i = 0; i < 3; i++) {
                original_size += original_pitch[i].x * original_pitch[i].y;
            }
            original_buffer.resize(original_size);
            original_data = original_buffer.data();
        } else {
            original_data = data;
        }

        // Implement YUV image align to pitch
        for (int plane = 0; plane < 3; plane++) {
            if (!frame->data[plane])
                continue;

            int src_width = frame->width;
            int src_height = frame->height;

            if (plane > 0) {
                switch (color_space_out) {
                case COLORSPACE_YUV444P:
                    break;
                case COLORSPACE_YUV440P:
                    src_height /= 2;
                    break;
                case COLORSPACE_YUV441P:
                    src_height /= 4;
                    break;
                case COLORSPACE_YUV422P:
                    src_width /= 2;
                    break;
                case COLORSPACE_YUV420P:
                    src_width /= 2;
                    src_height /= 2;
                    break;
                case COLORSPACE_YUV411P:
                    src_width /= 4;
                    break;
                case COLORSPACE_GRAYSCALE:
                    break;
                default:
                    LOG_WARN("An attempt was made to use an unsupported color space.");
                    return false;
                }
            }

            for (int y = 0; y < src_height; y++) {
                const uint8_t *src_row = frame->data[plane] + y * frame->linesize[plane];
                uint8_t *dst_row = original_data + y * original_pitch[plane].x;

                // Copy actual pixel data
                std::memcpy(dst_row, src_row, src_width);

                // Fill the rest with the last pixel value
                uint8_t last_pixel = src_row[src_width - 1];
                std::fill(dst_row + src_width, dst_row + original_pitch[plane].x, last_pixel);
            }

            // Fill remaining rows with the last row
            const uint8_t *last_row = original_data + (src_height - 1) * original_pitch[plane].x;
            for (int y = src_height; y < original_pitch[plane].y; y++) {
                uint8_t *dst_row = original_data + y * original_pitch[plane].x;
                std::memcpy(dst_row, last_row, original_pitch[plane].x);
            }

            // Move to the next plane
            original_data += original_pitch[plane].x * original_pitch[plane].y;
        }

        if (this->downscale_ratio != 1) {
            // The image downscaling on PS Vita is not based on width and height scaling, but rather on pitch size.
            // When attempting to emulate this using sws_scale, the code complexity increases significantly. Therefore, the implementation has been simplified.

            original_data = original_buffer.data();

            for (int plane = 0; plane < 3; plane++) {
                if (!frame->data[plane])
                    continue;

                int src_width = original_pitch[plane].x;
                int src_height = original_pitch[plane].y;
                int dst_width = this->pitch[plane].x;
                int dst_height = this->pitch[plane].y;

                for (int y = 0; y < dst_height; y++) {
                    for (int x = 0; x < dst_width; x++) {
                        int sum = 0;
                        int count = 0;
                        int src_y_start = y * this->downscale_ratio;
                        int src_y_end = std::min(src_y_start + this->downscale_ratio, src_height);
                        int src_x_start = x * this->downscale_ratio;
                        int src_x_end = std::min(src_x_start + this->downscale_ratio, src_width);

                        for (int sy = src_y_start; sy < src_y_end; sy++) {
                            for (int sx = src_x_start; sx < src_x_end; sx++) {
                                sum += original_data[sy * original_pitch[plane].x + sx];
                                count++;
                            }
                        }

                        if (count > 0) {
                            data[y * this->pitch[plane].x + x] = sum / count;
                        }
                    }
                }
                data += this->pitch[plane].x * this->pitch[plane].y;
                original_data += original_pitch[plane].x * original_pitch[plane].y;
            }
        }
    }

    if (size) {
        size->width = original_width;
        size->height = original_height;
    }

    av_frame_free(&frame);

    return true;
}

DecoderColorSpace MjpegDecoderState::get_color_space() {
    return this->color_space_out;
}

void MjpegDecoderState::get_pitch_info(MJpegPitch pitch[4]) {
    for (int i = 0; i < 4; i++) {
        pitch[i] = this->pitch[i];
    }
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
