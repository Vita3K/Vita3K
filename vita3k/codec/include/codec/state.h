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

#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <queue>
#include <string>

struct AVFrame;
struct AVPacket;
struct AVIOContext;
struct AVCodecContext;
struct AVFormatContext;
struct AVCodecParserContext;
struct AVCodec;
struct SwrContext;

union DecoderSize {
    struct {
        uint32_t width;
        uint32_t height;
    };
    struct {
        uint32_t samples;
    };
};

// glGet sort of API to use virtual stuff
enum class DecoderQuery {
    // Video
    WIDTH,
    HEIGHT,

    // Audio
    CHANNELS,
    BIT_RATE,
    SAMPLE_RATE,

    AT9_SAMPLE_PER_FRAME,
    AT9_SAMPLE_PER_SUPERFRAME,
    AT9_FRAMES_IN_SUPERFRAME,
    AT9_SUPERFRAME_SIZE,
};

struct DecoderState {
    AVCodecContext *context{};

    virtual uint32_t get(DecoderQuery query);

    virtual void flush();
    virtual bool send(const uint8_t *data, uint32_t size) = 0;
    virtual bool receive(uint8_t *data, DecoderSize *size = nullptr) = 0;
    virtual uint32_t get_es_size();

    virtual ~DecoderState();
};

struct H264DecoderOptions {
    uint32_t pts_upper;
    uint32_t pts_lower;
    uint32_t dts_upper;
    uint32_t dts_lower;
};

struct H264DecoderState : public DecoderState {
    AVCodecParserContext *parser{};

    uint32_t width_out = 0;
    uint32_t height_out = 0;

    uint64_t pts = ~0ull;
    uint64_t dts = ~0ull;
    uint64_t pts_out = ~0ull;

    bool is_stopped = true;

    static uint32_t buffer_size(DecoderSize size);

    uint32_t get(DecoderQuery query) override;

    bool send(const uint8_t *data, uint32_t size) override;
    bool receive(uint8_t *data, DecoderSize *size = nullptr) override;
    void configure(void *options);
    void get_res(uint32_t &width, uint32_t &height);
    void get_pts(uint32_t &upper, uint32_t &lower);

    H264DecoderState(uint32_t width, uint32_t height);
    ~H264DecoderState() override;
};

struct MjpegDecoderState : public DecoderState {
    bool send(const uint8_t *data, uint32_t size) override;
    bool receive(uint8_t *data, DecoderSize *size) override;

    MjpegDecoderState();
};

struct Atrac9DecoderState : public DecoderState {
    uint32_t config_data;
    void *decoder_handle;
    void *atrac9_info;
    uint32_t es_size_used;
    std::vector<uint8_t> result;
    int superframe_frame_idx;
    int superframe_data_left;

    uint32_t get(DecoderQuery query) override;
    uint32_t get_es_size() override;

    bool send(const uint8_t *data, uint32_t size) override;
    bool receive(uint8_t *data, DecoderSize *size) override;
    void flush() override;

    explicit Atrac9DecoderState(uint32_t config_data);
    ~Atrac9DecoderState() override;
};

struct Mp3DecoderState : public DecoderState {
    const AVCodec *codec;
    uint32_t es_size_used;

    uint32_t get(DecoderQuery query) override;
    uint32_t get_es_size() override;

    bool send(const uint8_t *data, uint32_t size) override;
    bool receive(uint8_t *data, DecoderSize *size) override;

    explicit Mp3DecoderState(uint32_t channels);
    ~Mp3DecoderState() override;
};

struct ADPCMHistory {
    int32_t hist1;
    int32_t hist2;
    int32_t hist3;
    int32_t hist4;
};

struct PCMDecoderState : public DecoderState {
private:
    std::vector<std::uint8_t> final_result;
    float dest_frequency;
    SwrContext *swr_mono_to_stereo = nullptr;
    SwrContext *swr_stereo = nullptr;

public:
    // there are at most 2 channels
    ADPCMHistory adpcm_history[2] = {};

    std::uint32_t source_channels;

    /**
     * @brief Playback rate of the input audio stream without modifications
     */
    float source_frequency;

    /**
     * @brief State of utilization of Sony's HEVAG (High Efficiency VAG) ADPCM.
     */
    bool he_adpcm;

    bool send(const uint8_t *data, uint32_t size) override;
    bool receive(uint8_t *data, DecoderSize *size) override;

    explicit PCMDecoderState(const float dest_frequency);
    ~PCMDecoderState();
};

struct AacDecoderState : public DecoderState {
    const AVCodec *codec;
    SwrContext *swr = nullptr;
    AVFrame *frame;
    uint32_t es_size_used;
    uint32_t get(DecoderQuery query) override;

    bool send(const uint8_t *data, uint32_t size) override;
    bool receive(uint8_t *data, DecoderSize *size) override;
    uint32_t get_es_size() override;

    explicit AacDecoderState(uint32_t sample_rate, uint32_t channels);
    ~AacDecoderState();
};

struct PlayerState {
    std::string video_playing;
    std::queue<std::string> videos_queue;

    AVFormatContext *format{};
    AVCodecContext *video_context{};
    AVCodecContext *audio_context{};
    int32_t video_stream_id = -1;
    int32_t audio_stream_id = -1;

    std::queue<AVPacket *> audio_packets;
    std::queue<AVPacket *> video_packets;

    uint64_t time_of_last_frame = 0;
    uint64_t framerate_microseconds = 0;

    uint64_t last_timestamp = 0;
    uint32_t last_channels = 0;
    uint32_t last_sample_rate = 0;
    uint32_t last_sample_count = 0;

    DecoderSize get_size();
    uint64_t get_framerate_microseconds();

    void pop_video();
    void free_video();
    void switch_video(const std::string &path);

    bool next_packet(int32_t stream_id);

    std::vector<int16_t> receive_audio();
    std::vector<uint8_t> receive_video();

    void queue(const std::string &path);

    ~PlayerState();
};

void convert_rgb_to_yuv(const uint8_t *rgba, uint8_t *yuv, uint32_t width, uint32_t height, const bool is_yuv420);
void convert_yuv_to_rgb(const uint8_t *yuv, uint8_t *rgba, uint32_t width, uint32_t height, const bool is_yuv420);
int convert_yuv_to_jpeg(const uint8_t *yuv, uint8_t *jpeg, uint32_t width, uint32_t height, uint32_t max_size, bool is_yuv420);
void copy_yuv_data_from_frame(AVFrame *frame, uint8_t *dest);
std::string codec_error_name(int error);
