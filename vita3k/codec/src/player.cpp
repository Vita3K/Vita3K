#include <codec/state.h>

#include <util/fs.h>
#include <util/log.h>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

#include <cassert>
#include <chrono>

uint64_t PlayerState::get_framerate_microseconds() {
    AVRational rational = format->streams[video_stream_id]->avg_frame_rate;
    return static_cast<float>(rational.den) / static_cast<float>(rational.num) * 1000000;
}

DecoderSize PlayerState::get_size() {
    if (video_context)
        return { static_cast<uint32_t>(video_context->width), static_cast<uint32_t>(video_context->height) };

    return {};
}

void PlayerState::pop_video() {
    switch_video(videos_queue.front());
    videos_queue.pop();
}

void PlayerState::free_video() {
    if (video_context) {
        avcodec_close(video_context);
        avcodec_free_context(&video_context);
    }

    if (audio_context) {
        avcodec_close(audio_context);
        avcodec_free_context(&audio_context);
    }

    if (format) {
        avformat_close_input(&format);
    }

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

    video_playing = "";
}

void PlayerState::switch_video(const std::string &path) {
    free_video();
    video_playing = path;

    int error;

    error = avformat_open_input(&format, path.c_str(), nullptr, nullptr);
    assert(error == 0);

    // Load stream info.
    error = avformat_find_stream_info(format, nullptr);
    assert(error >= 0);

    video_stream_id = av_find_best_stream(format, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    audio_stream_id = av_find_best_stream(format, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);

    if (video_stream_id >= 0) {
        AVStream *video_stream = format->streams[video_stream_id];
        AVCodec *video_codec = avcodec_find_decoder(video_stream->codecpar->codec_id);
        video_context = avcodec_alloc_context3(video_codec);
        avcodec_parameters_to_context(video_context, video_stream->codecpar);
        avcodec_open2(video_context, video_codec, nullptr);
    }

    if (audio_stream_id >= 0) {
        AVStream *audio_stream = format->streams[audio_stream_id];
        AVCodec *audio_codec = avcodec_find_decoder(audio_stream->codecpar->codec_id);
        audio_context = avcodec_alloc_context3(audio_codec);
        avcodec_parameters_to_context(audio_context, audio_stream->codecpar);
        avcodec_open2(audio_context, audio_codec, nullptr);
    }
}

bool PlayerState::next_packet(int32_t stream_id) {
    std::queue<AVPacket *> &this_queue = stream_id == video_stream_id ? video_packets : audio_packets;
    std::queue<AVPacket *> &other_queue = stream_id != video_stream_id ? video_packets : audio_packets;

    while (true) {
        if (!this_queue.empty()) {
            AVPacket *this_packet = this_queue.front();
            this_queue.pop();

            if (stream_id == video_stream_id) {
                int err = avcodec_send_packet(video_context, this_packet);
                assert(err == 0);
            }

            if (stream_id == audio_stream_id) {
                int err = avcodec_send_packet(audio_context, this_packet);
                assert(err == 0);
            }

            av_packet_free(&this_packet);
            return true;
        }

        AVPacket *packet = av_packet_alloc();
        if (av_read_frame(format, packet) != 0)
            return false;

        if (packet->stream_index == stream_id) {
            this_queue.push(packet);
        } else {
            other_queue.push(packet);
        }
    }
}

std::vector<int16_t> PlayerState::receive_audio() {
    if (audio_stream_id < 0)
        return {};

    if (video_playing.empty())
        return {};

    int error;
    AVFrame *frame = av_frame_alloc();
    std::vector<int16_t> data;
    while (true) {
        error = avcodec_receive_frame(audio_context, frame);

        if (error == AVERROR(EAGAIN) && next_packet(audio_stream_id))
            continue;

        if (error != 0) {
            if (videos_queue.empty()) {
                // Stop playing videos or
                video_playing = "";
                break;
            } else {
                // Play the next video (if there is any).
                switch_video(videos_queue.front());
                videos_queue.pop();
                continue;
            }
        }

        LOG_WARN_IF(frame->format != AV_SAMPLE_FMT_FLTP, "Unknown audio format {}.", frame->format);

        last_channels = frame->channels;
        last_sample_count = frame->nb_samples;
        last_sample_rate = frame->sample_rate;

        data.resize(frame->nb_samples * frame->channels);

        for (int a = 0; a < frame->nb_samples; a++) {
            for (int b = 0; b < frame->channels; b++) {
                auto *frame_data = reinterpret_cast<float *>(frame->data[b]);
                float current_sample = frame_data[a];
                int16_t pcm_sample = current_sample * INT16_MAX;

                data[a * frame->channels + b] = pcm_sample;
            }
        }

        break;
    }

    av_frame_free(&frame);
    return data;
}

std::vector<uint8_t> PlayerState::receive_video() {
    if (video_stream_id < 0)
        return {};

    if (video_playing.empty())
        return {};

    int error;
    AVFrame *frame = av_frame_alloc();
    std::vector<uint8_t> data;
    while (true) {
        error = avcodec_receive_frame(video_context, frame);

        if (error == AVERROR(EAGAIN) && next_packet(video_stream_id))
            continue;

        if (error != 0) {
            if (videos_queue.empty()) {
                // Stop playing videos or
                video_playing = "";
                break;
            } else {
                // Play the next video (if there is any).
                switch_video(videos_queue.front());
                videos_queue.pop();
                continue;
            }
        }

        last_timestamp = frame->best_effort_timestamp;

        data.resize(H264DecoderState::buffer_size(
            { static_cast<uint32_t>(video_context->width), static_cast<uint32_t>(video_context->height) }));
        copy_yuv_data_from_frame(frame, data.data());

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

    video_playing = "";
    videos_queue = {};
}
