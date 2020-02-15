#pragma once

#include <array>
#include <queue>
#include <string>
#include <cstdint>

struct AVFrame;
struct AVPacket;
struct AVCodecContext;
struct AVFormatContext;
struct AVCodecParserContext;

struct DecoderSize {
    uint32_t width;
    uint32_t height;
};

struct DecoderState {
    AVCodecContext *context{};

    virtual DecoderSize get_size();

    virtual void flush();
    virtual void send(const uint8_t *data, uint32_t size) = 0;
    virtual void receive(uint8_t *data, DecoderSize *size = nullptr) = 0;
    virtual void configure(void *options);

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

    DecoderSize get_size() override;

    void send(const uint8_t *data, uint32_t size) override;
    void receive(uint8_t *data, DecoderSize *size) override;
    void configure(void *options) override;

    H264DecoderState(uint32_t width, uint32_t height);
    ~H264DecoderState() override;
};

struct MjpegDecoderState : public DecoderState {
    void send(const uint8_t *data, uint32_t size) override;
    void receive(uint8_t *data, DecoderSize *size) override;

    MjpegDecoderState();
};

struct Atrac9DecoderState : public DecoderState {
    AVCodecContext *context{};

    Atrac9DecoderState();
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
