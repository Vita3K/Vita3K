#include <codec/state.h>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
}

#include <util/log.h>

#include <cassert>

void convert_yuv_to_rgb(const uint8_t *yuv, uint8_t *rgba, uint32_t width, uint32_t height) {
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
        LOG_WARN("Error sending Mjpeg packet: {}.", log_hex(static_cast<uint32_t>(error)));
        return false;
    }
    return true;
}

bool MjpegDecoderState::receive(uint8_t *data, DecoderSize *size) {
    AVFrame *frame = av_frame_alloc();
    int error = avcodec_receive_frame(context, frame);
    if (error < 0) {
        LOG_WARN("Error receiving Mjpeg frame: {}.", log_hex(static_cast<uint32_t>(error)));
        av_frame_free(&frame);
        return false;
    }

    if (data) {
        uint8_t *channels[] = {
            &data[0], // y
            &data[frame->width * frame->height], // u
            &data[frame->width * frame->height * 2], // v
        };

        assert(frame->format == AV_PIX_FMT_YUVJ444P);

        // Copy YUV444 data.
        for (uint32_t a = 0; a < 3; a++) {
            for (int b = 0; b < frame->height; b++) {
                std::memcpy(&channels[a][b * frame->width], &frame->data[a][b * frame->linesize[a]], frame->width);
            }
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
    AVCodec *codec = avcodec_find_decoder(AV_CODEC_ID_MJPEG);
    assert(codec);

    context = avcodec_alloc_context3(codec);
    assert(context);
    int error = avcodec_open2(context, codec, nullptr);
    assert(error == 0);
}
