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

#include <util/fs.h>
#include <util/log.h>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/mathematics.h>
}

#include <cassert>

uint64_t PlayerState::get_framerate_microseconds() {
    if (!format || video_stream_id < 0)
        return 0;
    AVRational rational = av_guess_frame_rate(format, format->streams[video_stream_id], nullptr);
    if (rational.num <= 0 || rational.den <= 0)
        return 0;
    return av_rescale_q(1, av_inv_q(rational), AVRational{ 1, 1000000 });
}

DecoderSize PlayerState::get_size() {
    if (video_context)
        return { { static_cast<uint32_t>(video_context->width), static_cast<uint32_t>(video_context->height) } };

    return {};
}

void PlayerState::pop_video() {
    switch_video(videos_queue.front());
    videos_queue.pop();
}

void PlayerState::free_video() {
    if (video_context)
        avcodec_free_context(&video_context);

    if (audio_context)
        avcodec_free_context(&audio_context);

    if (format)
        avformat_close_input(&format);

    while (!video_packets.empty()) {
        AVPacket *packet = video_packets.front();
        av_packet_free(&packet);
        video_packets.pop();
    }

    while (!audio_packets.empty()) {
        AVPacket *packet = audio_packets.front();
        av_packet_free(&packet);
        audio_packets.pop();
    }

    video_playing.clear();
    video_stream_id = audio_stream_id = -1;
    demux_eof = video_draining = audio_draining = false;
    video_eof = audio_eof = true;
    last_timestamp = last_audio_timestamp = 0;
}

void PlayerState::switch_video(const std::string &path) {
    const std::string next_path = path;
    free_video();
    video_playing = next_path;

    int error = avformat_open_input(&format, next_path.c_str(), nullptr, nullptr);
    if (error < 0) {
        LOG_ERROR("Cannot open video '{}': {}", next_path, error);
        free_video();
        return;
    }

    // Load stream info.
    error = avformat_find_stream_info(format, nullptr);
    if (error < 0) {
        LOG_ERROR("Cannot read video stream info '{}': {}", next_path, error);
        free_video();
        return;
    }

    video_stream_id = av_find_best_stream(format, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    audio_stream_id = av_find_best_stream(format, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);

    if (video_stream_id >= 0) {
        AVStream *video_stream = format->streams[video_stream_id];
        const AVCodec *video_codec = avcodec_find_decoder(video_stream->codecpar->codec_id);
        video_context = avcodec_alloc_context3(video_codec);
        avcodec_parameters_to_context(video_context, video_stream->codecpar);
        if (avcodec_open2(video_context, video_codec, nullptr) < 0) {
            free_video();
            return;
        }
        video_eof = false;
    }

    if (audio_stream_id >= 0) {
        AVStream *audio_stream = format->streams[audio_stream_id];
        const AVCodec *audio_codec = avcodec_find_decoder(audio_stream->codecpar->codec_id);
        audio_context = avcodec_alloc_context3(audio_codec);
        avcodec_parameters_to_context(audio_context, audio_stream->codecpar);
        if (avcodec_open2(audio_context, audio_codec, nullptr) < 0) {
            free_video();
            return;
        }
        audio_eof = false;
    }
}

bool PlayerState::next_packet(int32_t stream_id) {
    if (!format)
        return false;
    AVCodecContext *context = stream_id == video_stream_id ? video_context : audio_context;
    if (!context)
        return false;
    auto &packets = stream_id == video_stream_id ? video_packets : audio_packets;
    bool &draining = stream_id == video_stream_id ? video_draining : audio_draining;

    while (true) {
        if (!packets.empty()) {
            AVPacket *packet = packets.front();
            packets.pop();
            const int error = avcodec_send_packet(context, packet);
            av_packet_free(&packet);
            return error >= 0;
        }

        if (demux_eof) {
            if (draining)
                return false;
            draining = true;
            return avcodec_send_packet(context, nullptr) >= 0;
        }

        AVPacket *packet = av_packet_alloc();
        if (!packet)
            return false;
        const int error = av_read_frame(format, packet);
        if (error < 0) {
            av_packet_free(&packet);
            demux_eof = true;
            if (error != AVERROR_EOF)
                LOG_WARN("Video demux stopped with error {}", error);
            continue;
        }
        if (packet->stream_index == video_stream_id)
            video_packets.push(packet);
        else if (packet->stream_index == audio_stream_id)
            audio_packets.push(packet);
        else
            av_packet_free(&packet);
    }
}

void PlayerState::finish_stream(int32_t stream_id) {
    if (stream_id == video_stream_id)
        video_eof = true;
    if (stream_id == audio_stream_id)
        audio_eof = true;
    // The shorter stream must not discard queued frames from the longer stream.
    if (!video_eof || !audio_eof)
        return;
    if (!videos_queue.empty())
        pop_video();
    else if (looping) {
        LOG_DEBUG("Looping video '{}'", video_playing);
        switch_video(video_playing);
    } else
        video_playing.clear();
}

std::vector<int16_t> PlayerState::receive_audio() {
    if (audio_stream_id < 0 || audio_eof || !audio_context)
        return {};

    if (video_playing.empty())
        return {};

    AVFrame *frame = av_frame_alloc();
    std::vector<int16_t> data;
    while (true) {
        int error = avcodec_receive_frame(audio_context, frame);

        if (error == AVERROR(EAGAIN) && next_packet(audio_stream_id))
            continue;

        if (error != 0) {
            finish_stream(audio_stream_id);
            break;
        }

        LOG_WARN_IF(frame->format != AV_SAMPLE_FMT_FLTP, "Unknown audio format {}.", frame->format);

        const int64_t timestamp = frame->best_effort_timestamp;
        last_audio_timestamp = timestamp == AV_NOPTS_VALUE || timestamp < 0 ? 0
                                                                            : av_rescale_q(timestamp, format->streams[audio_stream_id]->time_base, AVRational{ 1, 1000 });
        last_channels = frame->ch_layout.nb_channels;
        last_sample_count = frame->nb_samples;
        last_sample_rate = frame->sample_rate;

        data.resize(frame->nb_samples * frame->ch_layout.nb_channels);

        for (int a = 0; a < frame->nb_samples; a++) {
            for (int b = 0; b < frame->ch_layout.nb_channels; b++) {
                auto *frame_data = reinterpret_cast<float *>(frame->data[b]);
                float current_sample = frame_data[a];
                int16_t pcm_sample = current_sample * INT16_MAX;

                data[a * frame->ch_layout.nb_channels + b] = pcm_sample;
            }
        }

        break;
    }

    av_frame_free(&frame);
    return data;
}

std::vector<uint8_t> PlayerState::receive_video() {
    if (video_stream_id < 0 || video_eof || !video_context)
        return {};

    if (video_playing.empty())
        return {};

    AVFrame *frame = av_frame_alloc();
    std::vector<uint8_t> data;
    while (true) {
        int error = avcodec_receive_frame(video_context, frame);

        if (error == AVERROR(EAGAIN) && next_packet(video_stream_id))
            continue;

        if (error != 0) {
            finish_stream(video_stream_id);
            break;
        }

        const int64_t timestamp = frame->best_effort_timestamp;
        last_timestamp = timestamp == AV_NOPTS_VALUE || timestamp < 0 ? 0
                                                                      : av_rescale_q(timestamp, format->streams[video_stream_id]->time_base, AVRational{ 1, 1000 });

        data.resize(H264DecoderState::buffer_size(
            { { static_cast<uint32_t>(video_context->width), static_cast<uint32_t>(video_context->height) } }));
        copy_yuv_data_from_frame(frame, data.data(), frame->width, frame->height, false);

        break;
    }

    av_frame_free(&frame);
    return data;
}

void PlayerState::queue(const std::string &path) {
    if (fs::exists(path)) {
        LOG_INFO("Queued video: '{}'.", path);
        if (video_playing.empty())
            switch_video(path);
        else
            videos_queue.push(path);
    } else {
        LOG_INFO("Cannot find video: {}", path);
    }
}

PlayerState::~PlayerState() {
    free_video();

    video_playing.clear();
    videos_queue = {};
}
