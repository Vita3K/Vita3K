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

#pragma once

#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <optional>
#include <queue>
#include <span>
#include <string>
#include <vector>

struct AVFrame;
struct AVPacket;
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

enum DecoderColorSpace {
    COLORSPACE_UNKNOWN,
    COLORSPACE_GRAYSCALE,
    COLORSPACE_YUV444P,
    COLORSPACE_YUV440P,
    COLORSPACE_YUV441P,
    COLORSPACE_YUV422P,
    COLORSPACE_YUV420P,
    COLORSPACE_YUV411P,
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

    std::mutex codec_mutex;

    virtual uint32_t get(DecoderQuery query);

    virtual void flush();
    virtual bool send(const uint8_t *data, uint32_t size) = 0;
    virtual bool receive(uint8_t *data, DecoderSize *size = nullptr) = 0;
    virtual uint32_t get_es_size();

    virtual ~DecoderState();
};

std::optional<uint32_t> avc_library_memory_size(uint32_t width, uint32_t height, uint32_t references, uint32_t streams);
std::optional<uint32_t> m4v_library_memory_size(uint32_t width, uint32_t height, uint32_t references, uint32_t streams);
std::optional<uint32_t> m4v_input_buffer_capacity(uint32_t width, uint32_t height);

std::optional<std::vector<uint8_t>> h264_nal_to_annexb(std::span<const uint8_t> payload,
    std::span<const uint8_t> extra0, std::span<const uint8_t> extra1, uint32_t length_size);

struct H264DecodedFrame {
    uint32_t width = 0;
    uint32_t height = 0;
    uint64_t pts = ~0ull;
    bool interlaced = false;
    std::vector<uint8_t> yuv420;
};

bool h264_frame_to_yuv420(const AVFrame &frame, H264DecodedFrame &output);

enum class H264OutputFormat {
    yuv420p,
    nv12,
    rgba,
    bgra,
};

struct VideoCscSurface {
    std::span<uint8_t> data;
    uint32_t width;
    uint32_t height;
    uint32_t pitch;
    H264OutputFormat format;
};

bool video_convert_surface(const VideoCscSurface &source, const VideoCscSurface &destination,
    uint32_t coefficient, uint8_t alpha);

bool h264_render_frame(const H264DecodedFrame &frame, std::span<uint8_t> destination,
    uint32_t pitch, uint32_t surface_height, uint32_t width, uint32_t height,
    H264OutputFormat format, uint8_t alpha);

enum class H264AsyncResult {
    success,
    full,
    invalid_input,
    decode_error,
};

enum class M4vColorRange { unknown,
    limited,
    full };

class M4vDecodePipeline {
public:
    bool submit() {
        const bool previous_in_flight = in_flight;
        in_flight = true;
        return previous_in_flight;
    }
    void finish() { in_flight = false; }

private:
    bool in_flight = false;
};

struct M4vFrameMetadata {
    // Zero numerator/denominator means unknown. The colour IDs are FFmpeg/ISO
    // values, with -1 for unspecified; they are not firmware enum values.
    int sar_num = 0;
    int sar_den = 0;
    int color_primaries = -1;
    int color_transfer = -1;
    int color_matrix = -1;
    // Source range; h264_frame_to_yuv420 converts full-range pixels to limited range.
    M4vColorRange color_range = M4vColorRange::unknown;
    uint64_t output_pts = ~0ull;
};

// Calls must be serialized by the owner. A full result never consumes the input;
// retry it after taking frames. A decode_error may follow packet consumption:
// flush before reuse. A successful drain reaches EOF; flush starts a new stream.
// Packets must include MPEG-4 configuration/VOL data when required by the stream.
// PTS/DTS use ~0ull for unknown and otherwise carry signed FFmpeg timestamps bitwise.
class M4vDecoderState {
public:
    static constexpr size_t max_queued_frames = 16;
    static std::unique_ptr<M4vDecoderState> create(uint32_t width, uint32_t height);
    ~M4vDecoderState();
    M4vDecoderState(const M4vDecoderState &) = delete;
    M4vDecoderState &operator=(const M4vDecoderState &) = delete;

    H264AsyncResult send_packet(std::span<const uint8_t> data, uint64_t pts, uint64_t dts);
    H264AsyncResult input_status();
    H264AsyncResult drain();
    bool take_frame(H264DecodedFrame &frame, M4vFrameMetadata *metadata = nullptr);
    void flush();

private:
    explicit M4vDecoderState(AVCodecContext *context);
    H264AsyncResult collect_frames();
    AVCodecContext *context;
    struct QueuedFrame {
        H264DecodedFrame frame;
        M4vFrameMetadata metadata;
    };
    std::deque<QueuedFrame> frames;
    bool drain_requested = false;
    bool draining = false;
    bool drained = false;
    bool failed = false;
};

struct H264DecoderOptions {
    uint32_t pts_upper;
    uint32_t pts_lower;
    uint32_t dts_upper;
    uint32_t dts_lower;
};

struct H264DecoderState : public DecoderState {
    AVCodecParserContext *parser{};

    uint32_t width_in = 0;
    uint32_t height_in = 0;

    uint32_t width_out = 0;
    uint32_t height_out = 0;

    uint64_t pts = ~0ull;
    uint64_t dts = ~0ull;
    uint64_t pts_out = ~0ull;

    // true means the output format is yuv420p3, false means it is yuv420p2
    bool output_yuvp3;

    bool is_stopped = true;
    bool nal_drained = false;
    std::deque<H264DecodedFrame> decoded_frames;

    static uint32_t buffer_size(DecoderSize size);

    uint32_t get(DecoderQuery query) override;

    bool send(const uint8_t *data, uint32_t size) override;
    bool receive(uint8_t *data, DecoderSize *size = nullptr) override;
    void configure(void *options);
    void set_res(const uint32_t width, const uint32_t height);
    void get_res(uint32_t &width, uint32_t &height);
    void get_pts(uint32_t &upper, uint32_t &lower);
    void set_output_format(bool is_yuv_p3);

    H264AsyncResult send_nal(std::span<const uint8_t> payload, std::span<const uint8_t> extra0,
        std::span<const uint8_t> extra1, uint32_t length_size, uint64_t pts, uint64_t dts, size_t &produced);
    H264AsyncResult drain_nal(size_t &produced);
    bool take_frame(H264DecodedFrame &frame);
    void clear_frames();
    H264AsyncResult collect_frames(size_t &produced);

    H264DecoderState(uint32_t width, uint32_t height);
    ~H264DecoderState() override;
};

struct MJpegPitch {
    uint32_t x;
    uint32_t y;
};

struct MJpegDecoderOptions {
    bool use_standard_decoder;
    int downscale_ratio;
};

struct MjpegDecoderState : public DecoderState {
    bool use_standard_decoder = false;
    int downscale_ratio = 1;

    MJpegPitch pitch[4];

    DecoderColorSpace color_space_out;

    bool send(const uint8_t *data, uint32_t size) override;
    bool receive(uint8_t *data, DecoderSize *size) override;
    void configure(void *options);
    void get_pitch_info(MJpegPitch pitch[4]);
    DecoderColorSpace get_color_space();

    MjpegDecoderState();
};

struct Atrac9DecoderSavedState {
    double prev_values[2][256]{};
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

    bool send(const uint8_t *data, uint32_t size = 0) override;
    bool receive(uint8_t *data, DecoderSize *size) override;
    void flush() override;

    void export_state(Atrac9DecoderSavedState *dest);
    void load_state(const Atrac9DecoderSavedState *src);

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
    ~PCMDecoderState() override;
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
    ~AacDecoderState() override;
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

void convert_rgb_to_yuv(const uint8_t *rgba, uint8_t *yuv, uint32_t width, uint32_t height, const DecoderColorSpace color_space, bool is_bgra, int32_t in_pitch);
void convert_yuv_to_rgb(const uint8_t *yuv, uint8_t *rgba, uint32_t frame_width, const DecoderColorSpace color_space, bool is_bgra, MJpegPitch pitch[4]);
int convert_yuv_to_jpeg(const uint8_t *yuv, uint8_t *jpeg, uint32_t width, uint32_t height, uint32_t max_size, const DecoderColorSpace color_space, int32_t compress_ratio);
void copy_yuv_data_from_frame(AVFrame *frame, uint8_t *dest, const uint32_t width, const uint32_t height, bool is_p3);
void calculate_pitch_info(uint32_t width, uint32_t height, int downscale_ratio, DecoderColorSpace color_space, bool use_standard_decoder, MJpegPitch output_pitch[4]);
std::string codec_error_name(int error);
