// Vita3K emulator project
// Copyright (C) 2026 Vita3K team
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.

#include <codec/state.h>
#include <util/log.h>

extern "C" {
#include <libavcodec/avcodec.h>
}

#include <algorithm>
#include <bit>
#include <climits>
#include <cstring>
#include <new>

std::optional<uint32_t> m4v_input_buffer_capacity(uint32_t width, uint32_t height) {
    if (width < 64 || width > 1280 || height < 64 || height > 720 || ((width | height) & 15))
        return std::nullopt;
    return std::min((3 * width * height / 4 + 255) & ~255u, 691200u);
}

std::optional<uint32_t> m4v_library_memory_size(uint32_t width, uint32_t height, uint32_t references, uint32_t streams) {
    if (width < 64 || width > 1280 || height < 64 || height > 720 || ((width | height) & 15) || references > 16 || streams > 4)
        return std::nullopt;
    references = std::max(references, 1u);
    streams = std::max(streams, 1u);
    if (static_cast<uint64_t>(width) * height * 3 / 2 * references > 0x697800)
        return std::nullopt;

    const auto align64 = [](uint32_t value) { return (value + 63) & ~63u; };
    const auto align256 = [](uint32_t value) { return (value + 255) & ~255u; };
    constexpr uint32_t command_size = 256;
    const uint32_t job = 2 * align64(16 * command_size) + align64(command_size) + align64(8 * command_size) + align64(128 * command_size) + align64(544 * command_size) + 2 * align64(136 * command_size) + align64(256 * command_size) + 2 * align64(64 * command_size);
    constexpr uint32_t context = 64 + 2048 + 128 + 64 + 4096 + 16384 + 8192 + 1024 + 2048 + 1024;
    const uint32_t helper = 4 * job + 2 * (context + 3 * (width / 16 + 1) * 64) + 11392;
    const uint32_t work = align256(64 + 192 + 128 * streams + 64 + helper);
    const uint32_t picture = align256(3 * width * height / 4);
    const uint32_t decoder = picture < 81920 ? 92544 : picture + 10624;
    return (work + 1536 + 262911 + align256(188) + align256(116 * streams) + decoder * 2 * streams) & ~0x3ffffu;
}

M4vDecoderState::M4vDecoderState(AVCodecContext *context)
    : context(context) {}

M4vDecoderState::~M4vDecoderState() {
    avcodec_free_context(&context);
}

std::unique_ptr<M4vDecoderState> M4vDecoderState::create(uint32_t width, uint32_t height) {
    if (!width || !height || width > 1920 || height > 1088) {
        LOG_ERROR("Invalid MPEG-4 decoder dimensions {}x{}", width, height);
        return {};
    }
    const AVCodec *codec = avcodec_find_decoder(AV_CODEC_ID_MPEG4);
    if (!codec) {
        LOG_ERROR("MPEG-4 decoder is unavailable");
        return {};
    }
    AVCodecContext *context = avcodec_alloc_context3(codec);
    if (!context) {
        LOG_ERROR("Unable to allocate MPEG-4 decoder context");
        return {};
    }
    context->width = static_cast<int>(width);
    context->height = static_cast<int>(height);
    const int error = avcodec_open2(context, codec, nullptr);
    if (error < 0) {
        LOG_ERROR("Unable to open MPEG-4 decoder: {}", codec_error_name(error));
        avcodec_free_context(&context);
        return {};
    }
    auto state = std::unique_ptr<M4vDecoderState>(new (std::nothrow) M4vDecoderState(context));
    if (!state) {
        LOG_ERROR("Unable to allocate MPEG-4 decoder state");
        avcodec_free_context(&context);
    }
    return state;
}

H264AsyncResult M4vDecoderState::collect_frames() {
    while (frames.size() < max_queued_frames) {
        AVFrame *frame = av_frame_alloc();
        if (!frame) {
            LOG_ERROR("Unable to allocate MPEG-4 output frame");
            failed = true;
            return H264AsyncResult::decode_error;
        }
        const int error = avcodec_receive_frame(context, frame);
        if (error == AVERROR(EAGAIN) || error == AVERROR_EOF) {
            av_frame_free(&frame);
            if (error == AVERROR_EOF)
                drained = true;
            return H264AsyncResult::success;
        }
        if (error < 0) {
            LOG_ERROR("Unable to receive MPEG-4 frame: {}", codec_error_name(error));
            av_frame_free(&frame);
            failed = true;
            return H264AsyncResult::decode_error;
        }
        QueuedFrame output;
        const AVRational sar = frame->sample_aspect_ratio.num > 0 && frame->sample_aspect_ratio.den > 0
            ? frame->sample_aspect_ratio
            : context->sample_aspect_ratio;
        if (sar.num > 0 && sar.den > 0) {
            output.metadata.sar_num = sar.num;
            output.metadata.sar_den = sar.den;
        }
        const auto primaries = frame->color_primaries != AVCOL_PRI_UNSPECIFIED ? frame->color_primaries : context->color_primaries;
        const auto transfer = frame->color_trc != AVCOL_TRC_UNSPECIFIED ? frame->color_trc : context->color_trc;
        const auto matrix = frame->colorspace != AVCOL_SPC_UNSPECIFIED ? frame->colorspace : context->colorspace;
        if (primaries != AVCOL_PRI_UNSPECIFIED && primaries != AVCOL_PRI_RESERVED0)
            output.metadata.color_primaries = primaries;
        if (transfer != AVCOL_TRC_UNSPECIFIED && transfer != AVCOL_TRC_RESERVED0)
            output.metadata.color_transfer = transfer;
        if (matrix != AVCOL_SPC_UNSPECIFIED && matrix != AVCOL_SPC_RESERVED)
            output.metadata.color_matrix = matrix;
        const auto range = frame->color_range != AVCOL_RANGE_UNSPECIFIED ? frame->color_range : context->color_range;
        const auto format = static_cast<AVPixelFormat>(frame->format);
        if (format == AV_PIX_FMT_YUVJ420P || format == AV_PIX_FMT_YUVJ422P || format == AV_PIX_FMT_YUVJ444P || format == AV_PIX_FMT_YUVJ440P || format == AV_PIX_FMT_YUVJ411P)
            output.metadata.color_range = M4vColorRange::full;
        else if (range == AVCOL_RANGE_MPEG)
            output.metadata.color_range = M4vColorRange::limited;
        else if (range == AVCOL_RANGE_JPEG)
            output.metadata.color_range = M4vColorRange::full;
        const bool converted = h264_frame_to_yuv420(*frame, output.frame);
        output.metadata.output_pts = output.frame.pts;
        av_frame_free(&frame);
        if (!converted) {
            LOG_ERROR("Unable to convert MPEG-4 output frame");
            failed = true;
            return H264AsyncResult::decode_error;
        }
        frames.push_back(std::move(output));
    }
    return H264AsyncResult::full;
}

H264AsyncResult M4vDecoderState::send_packet(std::span<const uint8_t> data, uint64_t pts, uint64_t dts) {
    if (data.empty() || data.size() > INT_MAX - AV_INPUT_BUFFER_PADDING_SIZE) {
        LOG_ERROR("Invalid MPEG-4 input packet size {}", data.size());
        return H264AsyncResult::invalid_input;
    }
    if (failed || drain_requested || drained) {
        LOG_ERROR("MPEG-4 decoder requires a flush before sending packets");
        return H264AsyncResult::decode_error;
    }
    auto result = collect_frames();
    if (result != H264AsyncResult::success)
        return result;
    AVPacket *packet = av_packet_alloc();
    if (!packet) {
        LOG_ERROR("Unable to allocate MPEG-4 input packet");
        return H264AsyncResult::decode_error;
    }
    int error = av_new_packet(packet, static_cast<int>(data.size()));
    if (error >= 0) {
        std::memcpy(packet->data, data.data(), data.size());
        packet->pts = pts == ~0ull ? AV_NOPTS_VALUE : std::bit_cast<int64_t>(pts);
        packet->dts = dts == ~0ull ? AV_NOPTS_VALUE : std::bit_cast<int64_t>(dts);
        error = avcodec_send_packet(context, packet);
        if (error == AVERROR(EAGAIN)) {
            result = collect_frames();
            if (result == H264AsyncResult::success)
                error = avcodec_send_packet(context, packet);
        }
    }
    av_packet_free(&packet);
    if (result != H264AsyncResult::success)
        return result;
    if (error < 0) {
        LOG_ERROR("Unable to submit MPEG-4 packet: {}", codec_error_name(error));
        failed = true;
        return H264AsyncResult::decode_error;
    }
    result = collect_frames();
    // The packet was consumed, even if its output filled the queue.
    return result == H264AsyncResult::full ? H264AsyncResult::success : result;
}

H264AsyncResult M4vDecoderState::input_status() {
    if (failed)
        return H264AsyncResult::decode_error;
    if (drain_requested || drained)
        return H264AsyncResult::full;
    const auto result = collect_frames();
    return drained && result == H264AsyncResult::success ? H264AsyncResult::full : result;
}

H264AsyncResult M4vDecoderState::drain() {
    if (failed)
        return H264AsyncResult::decode_error;
    if (drained)
        return H264AsyncResult::success;
    drain_requested = true;
    auto result = collect_frames();
    if (result != H264AsyncResult::success || drained)
        return result;
    if (!draining) {
        int error = avcodec_send_packet(context, nullptr);
        if (error == AVERROR(EAGAIN)) {
            result = collect_frames();
            if (result != H264AsyncResult::success)
                return result;
            error = avcodec_send_packet(context, nullptr);
        }
        if (error == AVERROR_EOF) {
            drained = true;
            return H264AsyncResult::success;
        }
        if (error < 0) {
            LOG_ERROR("Unable to drain MPEG-4 decoder: {}", codec_error_name(error));
            failed = true;
            return H264AsyncResult::decode_error;
        }
        draining = true;
    }
    result = collect_frames();
    if (result == H264AsyncResult::success && !drained) {
        LOG_ERROR("MPEG-4 decoder requested input after drain");
        failed = true;
        return H264AsyncResult::decode_error;
    }
    return result;
}

bool M4vDecoderState::take_frame(H264DecodedFrame &frame, M4vFrameMetadata *metadata) {
    if (frames.empty())
        return false;
    frame = std::move(frames.front().frame);
    if (metadata)
        *metadata = frames.front().metadata;
    frames.pop_front();
    return true;
}

void M4vDecoderState::flush() {
    avcodec_flush_buffers(context);
    frames.clear();
    drain_requested = false;
    draining = false;
    drained = false;
    failed = false;
}
