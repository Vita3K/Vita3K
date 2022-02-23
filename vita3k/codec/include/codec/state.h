// Vita3K emulator project
// Copyright (C) 2022 Vita3K team
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
#include <queue>
#include <string>

struct AVFrame;
struct AVPacket;
struct AVCodecContext;
struct AVFormatContext;
struct AVCodecParserContext;

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

    AT9_BLOCK_ALIGN,
    AT9_SAMPLE_PER_SUPERFRAME,
    AT9_FRAMES_IN_SUPERFRAME,
    AT9_SUPERFRAME_SIZE,
};

struct DecoderState {
    AVCodecContext *context{};

    virtual uint32_t get(DecoderQuery query);

    // TODO: proper error handling (return bool?)
    virtual void flush();
    virtual bool send(const uint8_t *data, uint32_t size) = 0;
    virtual bool receive(uint8_t *data, DecoderSize *size = nullptr) = 0;
    virtual void configure(void *options);
    virtual uint32_t get_es_size(const uint8_t *data);

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

    uint64_t pts = ~0ull;
    uint64_t dts = ~0ull;

    static uint32_t buffer_size(DecoderSize size);

    uint32_t get(DecoderQuery query) override;

    bool send(const uint8_t *data, uint32_t size) override;
    bool receive(uint8_t *data, DecoderSize *size) override;
    void configure(void *options) override;

    H264DecoderState(uint32_t width, uint32_t height);
    ~H264DecoderState() override;
};

struct MjpegDecoderState : public DecoderState {
    bool send(const uint8_t *data, uint32_t size) override;
    bool receive(uint8_t *data, DecoderSize *size) override;

    MjpegDecoderState();
};

struct Atrac9DecoderState : public DecoderState {
    void *decoder_handle;
    uint32_t config_data;

    std::vector<std::uint8_t> result;

    uint32_t get_channel_count();
    uint32_t get_samples_per_superframe();
    uint32_t get_block_align();
    uint32_t get_frames_in_superframe();
    uint32_t get_superframe_size();

    uint32_t get(DecoderQuery query) override;

    bool send(const uint8_t *data, uint32_t size) override;
    bool receive(uint8_t *data, DecoderSize *size) override;

    explicit Atrac9DecoderState(uint32_t config_data);
    ~Atrac9DecoderState() override;
};

struct Mp3DecoderState : public DecoderState {
    uint32_t get(DecoderQuery query) override;
    uint32_t get_es_size(const uint8_t *data) override;

    bool send(const uint8_t *data, uint32_t size) override;
    bool receive(uint8_t *data, DecoderSize *size) override;

    explicit Mp3DecoderState(uint32_t channels);
    ~Mp3DecoderState() override;
};

struct PCMDecoderState : public DecoderState {
private:
    std::vector<std::uint8_t> final_result;
    float dest_frequency;

    std::int32_t adpcm_history1;
    std::int32_t adpcm_history2;
    std::int32_t adpcm_history3;
    std::int32_t adpcm_history4;

public:
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

void convert_yuv_to_rgb(const uint8_t *yuv, uint8_t *rgba, uint32_t width, uint32_t height);
void copy_yuv_data_from_frame(AVFrame *frame, uint8_t *dest);
bool resample_s16_to_f32(const int16_t *source_s16, int32_t source_channels, uint32_t source_samples, uint32_t source_freq,
    float *dest_f32, uint32_t dest_samples, uint32_t dest_freq);
