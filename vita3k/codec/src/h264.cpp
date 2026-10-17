// Vita3K emulator project
// Copyright (C) 2026 Vita3K team
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
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}

#include <algorithm>
#include <cassert>
#include <climits>
#include <cstring>

std::optional<uint32_t> avc_library_memory_size(uint32_t width, uint32_t height, uint32_t references, uint32_t streams) {
    if (width < 64 || width > 1920 || height < 64 || height > 1088 || references > 16 || streams > 4)
        return std::nullopt;

    references = std::max(references, 1u);
    streams = std::max(streams, 1u);
    const uint64_t frame_size_twice = static_cast<uint64_t>(width) * height * 3;
    const uint64_t frame_size = frame_size_twice / 2;
    const uint64_t reference_size = frame_size_twice * references / 2;
    if (reference_size > 0xc00000)
        return std::nullopt;

    uint32_t per_stream;
    if (frame_size <= 0x4a400 && reference_size <= 0x1bd800)
        per_stream = 0x240000;
    else if (frame_size <= 0x151800 && reference_size <= 0x697800)
        per_stream = 0x580000;
    else
        per_stream = 0x980000;
    return per_stream * streams;
}

std::optional<std::vector<uint8_t>> h264_nal_to_annexb(std::span<const uint8_t> payload,
    std::span<const uint8_t> extra0, std::span<const uint8_t> extra1, uint32_t length_size) {
    if (length_size < 1 || length_size > 4)
        return std::nullopt;

    std::vector<uint8_t> result;
    const auto append_nal = [&result](std::span<const uint8_t> nal) {
        if (!nal.empty()) {
            result.insert(result.end(), { 0, 0, 0, 1 });
            result.insert(result.end(), nal.begin(), nal.end());
        }
    };
    append_nal(extra0);
    append_nal(extra1);
    while (!payload.empty()) {
        if (payload.size() < length_size)
            return std::nullopt;
        uint32_t nal_size = 0;
        for (uint32_t i = 0; i < length_size; ++i)
            nal_size = (nal_size << 8) | payload[i];
        payload = payload.subspan(length_size);
        if (nal_size == 0 || nal_size > payload.size())
            return std::nullopt;
        append_nal(payload.first(nal_size));
        payload = payload.subspan(nal_size);
    }
    return result;
}

static AVPixelFormat csc_pixel_format(H264OutputFormat format) {
    switch (format) {
    case H264OutputFormat::yuv420p: return AV_PIX_FMT_YUV420P;
    case H264OutputFormat::nv12: return AV_PIX_FMT_NV12;
    case H264OutputFormat::rgba: return AV_PIX_FMT_RGBA;
    case H264OutputFormat::bgra: return AV_PIX_FMT_BGRA;
    }
    return AV_PIX_FMT_NONE;
}

static bool csc_surface_planes(const VideoCscSurface &surface, uint8_t *planes[4], int strides[4]) {
    if (!surface.width || !surface.height || surface.width > surface.pitch || surface.width > 1920 || surface.height > 1088 || surface.pitch > 2048 || ((surface.width | surface.height | surface.pitch) & 1))
        return false;
    const bool rgb = surface.format == H264OutputFormat::rgba || surface.format == H264OutputFormat::bgra;
    const size_t luma_size = static_cast<size_t>(surface.pitch) * surface.height;
    const size_t required = rgb ? luma_size * 4 : luma_size * 3 / 2;
    if (surface.data.size() < required || csc_pixel_format(surface.format) == AV_PIX_FMT_NONE)
        return false;
    planes[0] = surface.data.data();
    strides[0] = static_cast<int>(surface.pitch * (rgb ? 4 : 1));
    if (!rgb) {
        planes[1] = planes[0] + luma_size;
        strides[1] = static_cast<int>(surface.format == H264OutputFormat::nv12 ? surface.pitch : surface.pitch / 2);
        if (surface.format == H264OutputFormat::yuv420p) {
            planes[2] = planes[1] + luma_size / 4;
            strides[2] = strides[1];
        }
    }
    return true;
}

bool video_convert_surface(const VideoCscSurface &source, const VideoCscSurface &destination,
    uint32_t coefficient, uint8_t alpha) {
    uint8_t *source_planes[4]{}, *destination_planes[4]{};
    int source_strides[4]{}, destination_strides[4]{};
    if (coefficient > 2 || !csc_surface_planes(source, source_planes, source_strides) || !csc_surface_planes(destination, destination_planes, destination_strides)) {
        LOG_ERROR("Invalid video colour conversion surfaces or coefficient");
        return false;
    }
    const AVPixelFormat source_format = csc_pixel_format(source.format);
    const AVPixelFormat destination_format = csc_pixel_format(destination.format);
    uint8_t *input[4]{}, *output[4]{};
    int input_strides[4]{}, output_strides[4]{};
    if (av_image_alloc(input, input_strides, source.width, source.height, source_format, 32) < 0) {
        LOG_ERROR("Unable to allocate video conversion input");
        return false;
    }
    if (av_image_alloc(output, output_strides, destination.width, destination.height, destination_format, 32) < 0) {
        LOG_ERROR("Unable to allocate video conversion output");
        av_freep(&input[0]);
        return false;
    }
    const uint8_t *input_planes[] = { source_planes[0], source_planes[1], source_planes[2], nullptr };
    av_image_copy(input, input_strides, input_planes, source_strides, source_format, source.width, source.height);
    SwsContext *sws = sws_getContext(source.width, source.height, source_format,
        destination.width, destination.height, destination_format, SWS_BILINEAR, nullptr, nullptr, nullptr);
    bool success = sws != nullptr;
    if (success && coefficient) {
        const int *coefficients = sws_getCoefficients(coefficient == 2 ? SWS_CS_ITU709 : SWS_CS_ITU601);
        success = sws_setColorspaceDetails(sws, coefficients, 0, coefficients, 0, 0, 1 << 16, 1 << 16) >= 0;
    }
    if (success)
        success = sws_scale(sws, input, input_strides, 0, source.height, output, output_strides) == static_cast<int>(destination.height);
    if (success) {
        const uint8_t *output_planes[] = { output[0], output[1], output[2], nullptr };
        av_image_copy(destination_planes, destination_strides, output_planes, output_strides,
            destination_format, destination.width, destination.height);
        if (destination.format == H264OutputFormat::rgba || destination.format == H264OutputFormat::bgra)
            for (uint32_t y = 0; y < destination.height; ++y)
                for (uint32_t x = 0; x < destination.width; ++x)
                    destination_planes[0][static_cast<size_t>(y) * destination_strides[0] + x * 4 + 3] = alpha;
    } else {
        LOG_ERROR("Unable to convert video colour surfaces");
    }
    sws_freeContext(sws);
    av_freep(&input[0]);
    av_freep(&output[0]);
    return success;
}

bool h264_render_frame(const H264DecodedFrame &frame, std::span<uint8_t> destination,
    uint32_t pitch, uint32_t surface_height, uint32_t width, uint32_t height,
    H264OutputFormat format, uint8_t alpha) {
    if (!frame.width || !frame.height || !width || !height || width > pitch || height > surface_height || frame.width > 1920 || frame.height > 1088 || pitch > 2048 || surface_height > 1088)
        return false;
    const size_t source_y = static_cast<size_t>(frame.width) * frame.height;
    const size_t source_uv = static_cast<size_t>((frame.width + 1) / 2) * ((frame.height + 1) / 2);
    if (frame.yuv420.size() != source_y + 2 * source_uv)
        return false;

    const bool rgb = format == H264OutputFormat::rgba || format == H264OutputFormat::bgra;
    const size_t luma_size = static_cast<size_t>(pitch) * surface_height;
    if ((!rgb && ((pitch | surface_height) & 1)) || destination.size() < (rgb ? 4 * luma_size : luma_size * 3 / 2))
        return false;
    AVPixelFormat pixel_format;
    switch (format) {
    case H264OutputFormat::yuv420p: pixel_format = AV_PIX_FMT_YUV420P; break;
    case H264OutputFormat::nv12: pixel_format = AV_PIX_FMT_NV12; break;
    case H264OutputFormat::rgba: pixel_format = AV_PIX_FMT_RGBA; break;
    case H264OutputFormat::bgra: pixel_format = AV_PIX_FMT_BGRA; break;
    default: return false;
    }
    uint8_t *source[4]{};
    int source_pitches[4]{};
    uint8_t *planes[4]{};
    int pitches[4]{};
    if (av_image_alloc(source, source_pitches, frame.width, frame.height, AV_PIX_FMT_YUV420P, 32) < 0) {
        LOG_ERROR("Unable to allocate AVC source conversion buffer");
        return false;
    }
    if (av_image_alloc(planes, pitches, width, height, pixel_format, 32) < 0) {
        LOG_ERROR("Unable to allocate AVC output conversion buffer");
        av_freep(&source[0]);
        return false;
    }
    const uint8_t *input[] = { frame.yuv420.data(), frame.yuv420.data() + source_y,
        frame.yuv420.data() + source_y + source_uv, nullptr };
    const int input_pitches[] = { static_cast<int>(frame.width), static_cast<int>((frame.width + 1) / 2),
        static_cast<int>((frame.width + 1) / 2), 0 };
    av_image_copy(source, source_pitches, input, input_pitches, AV_PIX_FMT_YUV420P, frame.width, frame.height);
    SwsContext *sws = sws_getContext(frame.width, frame.height, AV_PIX_FMT_YUV420P,
        width, height, pixel_format, SWS_BILINEAR, nullptr, nullptr, nullptr);
    if (!sws) {
        LOG_ERROR("Unable to create AVC output conversion");
        av_freep(&source[0]);
        av_freep(&planes[0]);
        return false;
    }
    const int rows = sws_scale(sws, source, source_pitches, 0, frame.height, planes, pitches);
    sws_freeContext(sws);
    av_freep(&source[0]);
    if (rows != static_cast<int>(height)) {
        LOG_ERROR("Unable to convert AVC output: {} of {} rows", rows, height);
        av_freep(&planes[0]);
        return false;
    }
    for (uint32_t y = 0; y < height; ++y) {
        uint8_t *row = destination.data() + static_cast<size_t>(y) * pitch * (rgb ? 4 : 1);
        std::memcpy(row, planes[0] + static_cast<size_t>(y) * pitches[0], width * (rgb ? 4 : 1));
        if (rgb && alpha != 255)
            for (uint32_t x = 0; x < width; ++x)
                row[x * 4 + 3] = alpha;
    }
    if (!rgb) {
        const uint32_t chroma_width = (width + 1) / 2;
        const uint32_t chroma_height = (height + 1) / 2;
        const size_t chroma_pitch = format == H264OutputFormat::yuv420p ? pitch / 2 : pitch;
        const size_t row_size = format == H264OutputFormat::yuv420p ? chroma_width : chroma_width * 2;
        for (uint32_t y = 0; y < chroma_height; ++y) {
            std::memcpy(destination.data() + luma_size + y * chroma_pitch, planes[1] + y * pitches[1], row_size);
            if (format == H264OutputFormat::yuv420p)
                std::memcpy(destination.data() + luma_size + luma_size / 4 + y * chroma_pitch, planes[2] + y * pitches[2], row_size);
        }
    }
    av_freep(&planes[0]);
    return true;
}

bool h264_frame_to_yuv420(const AVFrame &frame, H264DecodedFrame &output) {
    if (frame.width <= 0 || frame.height <= 0 || frame.width > 1920 || frame.height > 1088) {
        LOG_ERROR("Unsupported AVC frame dimensions {}x{}", frame.width, frame.height);
        return false;
    }

    AVPixelFormat source_format = static_cast<AVPixelFormat>(frame.format);
    bool full_range = frame.color_range == AVCOL_RANGE_JPEG;
    switch (source_format) {
    case AV_PIX_FMT_YUVJ420P:
        source_format = AV_PIX_FMT_YUV420P;
        full_range = true;
        break;
    case AV_PIX_FMT_YUVJ422P:
        source_format = AV_PIX_FMT_YUV422P;
        full_range = true;
        break;
    case AV_PIX_FMT_YUVJ444P:
        source_format = AV_PIX_FMT_YUV444P;
        full_range = true;
        break;
    case AV_PIX_FMT_YUVJ440P:
        source_format = AV_PIX_FMT_YUV440P;
        full_range = true;
        break;
    case AV_PIX_FMT_YUVJ411P:
        source_format = AV_PIX_FMT_YUV411P;
        full_range = true;
        break;
    default: break;
    }

    output.width = static_cast<uint32_t>(frame.width);
    output.height = static_cast<uint32_t>(frame.height);
    output.pts = frame.pts == AV_NOPTS_VALUE ? ~0ull : static_cast<uint64_t>(frame.pts);
    output.interlaced = (frame.flags & AV_FRAME_FLAG_INTERLACED) != 0;
    const uint32_t chroma_width = (output.width + 1) / 2;
    const uint32_t chroma_height = (output.height + 1) / 2;
    const size_t luma_size = static_cast<size_t>(output.width) * output.height;
    const size_t chroma_size = static_cast<size_t>(chroma_width) * chroma_height;
    output.yuv420.resize(luma_size + 2 * chroma_size);
    uint8_t *planes[4]{};
    int pitches[4]{};
    if (av_image_alloc(planes, pitches, frame.width, frame.height, AV_PIX_FMT_YUV420P, 32) < 0) {
        LOG_ERROR("Unable to allocate AVC frame conversion buffer");
        return false;
    }
    SwsContext *sws = sws_getContext(frame.width, frame.height, source_format,
        frame.width, frame.height, AV_PIX_FMT_YUV420P, SWS_BILINEAR, nullptr, nullptr, nullptr);
    if (!sws) {
        LOG_ERROR("Unable to convert AVC frame format {} to YUV420", frame.format);
        av_freep(&planes[0]);
        return false;
    }
    if (full_range) {
        const int *coefficients = sws_getCoefficients(SWS_CS_DEFAULT);
        if (sws_setColorspaceDetails(sws, coefficients, 1, coefficients, 1, 0, 1 << 16, 1 << 16) < 0) {
            LOG_ERROR("Unable to configure the AVC full-range conversion");
            sws_freeContext(sws);
            av_freep(&planes[0]);
            return false;
        }
    }
    const int rows = sws_scale(sws, frame.data, frame.linesize, 0, frame.height, planes, pitches);
    sws_freeContext(sws);
    if (rows != static_cast<int>(output.height)) {
        LOG_ERROR("Unable to convert the complete AVC frame: {} of {} rows", rows, output.height);
        av_freep(&planes[0]);
        return false;
    }
    for (uint32_t y = 0; y < output.height; ++y)
        std::memcpy(output.yuv420.data() + static_cast<size_t>(y) * output.width, planes[0] + static_cast<size_t>(y) * pitches[0], output.width);
    for (uint32_t y = 0; y < chroma_height; ++y) {
        std::memcpy(output.yuv420.data() + luma_size + static_cast<size_t>(y) * chroma_width, planes[1] + static_cast<size_t>(y) * pitches[1], chroma_width);
        std::memcpy(output.yuv420.data() + luma_size + chroma_size + static_cast<size_t>(y) * chroma_width, planes[2] + static_cast<size_t>(y) * pitches[2], chroma_width);
    }
    av_freep(&planes[0]);
    if (full_range) {
        // swscale can bypass range conversion when the source and destination formats match.
        for (size_t i = 0; i < output.yuv420.size(); ++i) {
            const uint32_t scale = i < luma_size ? 219 : 224;
            output.yuv420[i] = static_cast<uint8_t>(16 + (output.yuv420[i] * scale + 127) / 255);
        }
    }
    return true;
}

H264AsyncResult H264DecoderState::collect_frames(size_t &produced) {
    constexpr size_t max_queued_frames = 16;
    while (decoded_frames.size() < max_queued_frames) {
        AVFrame *frame = av_frame_alloc();
        if (!frame) {
            LOG_ERROR("Unable to allocate an AVC output frame");
            return H264AsyncResult::decode_error;
        }
        const int error = avcodec_receive_frame(context, frame);
        if (error == AVERROR(EAGAIN) || error == AVERROR_EOF) {
            av_frame_free(&frame);
            return H264AsyncResult::success;
        }
        if (error < 0) {
            LOG_ERROR("Unable to receive an AVC frame: {}", codec_error_name(error));
            av_frame_free(&frame);
            return H264AsyncResult::decode_error;
        }
        H264DecodedFrame output;
        const bool converted = h264_frame_to_yuv420(*frame, output);
        av_frame_free(&frame);
        if (!converted)
            return H264AsyncResult::decode_error;
        decoded_frames.push_back(std::move(output));
        ++produced;
    }
    return H264AsyncResult::success;
}

H264AsyncResult H264DecoderState::send_nal(std::span<const uint8_t> payload, std::span<const uint8_t> extra0,
    std::span<const uint8_t> extra1, uint32_t length_size, uint64_t pts, uint64_t dts, size_t &produced) {
    produced = 0;
    auto packet_data = h264_nal_to_annexb(payload, extra0, extra1, length_size);
    if (!packet_data || packet_data->size() > INT_MAX) {
        LOG_ERROR("Invalid length-prefixed AVC NAL input");
        return H264AsyncResult::invalid_input;
    }
    if (packet_data->empty())
        return H264AsyncResult::success;

    std::lock_guard<std::mutex> lock(codec_mutex);
    if (decoded_frames.size() >= 16)
        return H264AsyncResult::full;
    if (nal_drained) {
        avcodec_flush_buffers(context);
        nal_drained = false;
    }
    AVPacket *packet = av_packet_alloc();
    if (!packet) {
        LOG_ERROR("Unable to allocate an AVC input packet");
        return H264AsyncResult::decode_error;
    }
    int error = av_new_packet(packet, static_cast<int>(packet_data->size()));
    if (error >= 0) {
        std::memcpy(packet->data, packet_data->data(), packet_data->size());
        packet->pts = pts == ~0ull ? AV_NOPTS_VALUE : static_cast<int64_t>(pts);
        packet->dts = dts == ~0ull ? AV_NOPTS_VALUE : static_cast<int64_t>(dts);
        error = avcodec_send_packet(context, packet);
    }
    av_packet_free(&packet);
    if (error < 0) {
        LOG_ERROR("Unable to submit an AVC NAL packet: {}", codec_error_name(error));
        return H264AsyncResult::decode_error;
    }
    is_stopped = false;
    return collect_frames(produced);
}

H264AsyncResult H264DecoderState::drain_nal(size_t &produced) {
    produced = 0;
    std::lock_guard<std::mutex> lock(codec_mutex);
    if (decoded_frames.size() >= 16)
        return H264AsyncResult::full;
    const int error = avcodec_send_packet(context, nullptr);
    if (error < 0 && error != AVERROR_EOF) {
        LOG_ERROR("Unable to drain the AVC decoder: {}", codec_error_name(error));
        return H264AsyncResult::decode_error;
    }
    is_stopped = true;
    nal_drained = true;
    return collect_frames(produced);
}

bool H264DecoderState::take_frame(H264DecodedFrame &frame) {
    std::lock_guard<std::mutex> lock(codec_mutex);
    if (decoded_frames.empty())
        return false;
    frame = std::move(decoded_frames.front());
    decoded_frames.pop_front();
    return true;
}

void H264DecoderState::clear_frames() {
    std::lock_guard<std::mutex> lock(codec_mutex);
    decoded_frames.clear();
    nal_drained = false;
}

void copy_yuv_data_from_frame(AVFrame *frame, uint8_t *dest, const uint32_t width, const uint32_t height, bool is_p3) {
    for (size_t i = 0; i < height; i++) {
        memcpy(dest, &frame->data[0][frame->linesize[0] * i], width);
        dest += width;
    }

    if (is_p3) {
        for (size_t i = 0; i < height / 2; i++) {
            memcpy(dest, &frame->data[1][frame->linesize[1] * i], width / 2);
            dest += width / 2;
        }
        for (size_t i = 0; i < height / 2; i++) {
            memcpy(dest, &frame->data[2][frame->linesize[2] * i], width / 2);
            dest += width / 2;
        }
    } else {
        // p2 format, U and V are interleaved
        for (size_t i = 0; i < height / 2; i++) {
            const uint8_t *src_u = &frame->data[1][frame->linesize[1] * i];
            const uint8_t *src_v = &frame->data[2][frame->linesize[2] * i];
            for (size_t j = 0; j < width / 2; j++) {
                dest[0] = src_u[j];
                dest[1] = src_v[j];
                dest += 2;
            }
        }
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
    std::lock_guard<std::mutex> lock(codec_mutex);

    if (nal_drained) {
        avcodec_flush_buffers(context);
        nal_drained = false;
        decoded_frames.clear();
    }
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
        copy_yuv_data_from_frame(frame, data, width_in, height_in, output_yuvp3);
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
    auto *opt = static_cast<H264DecoderOptions *>(options);

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

void H264DecoderState::set_output_format(bool is_yuv_p3) {
    this->output_yuvp3 = is_yuv_p3;
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
