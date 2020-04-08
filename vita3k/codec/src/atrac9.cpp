#include <codec/state.h>

extern "C" {
#include <libavutil/opt.h>
#include <libavcodec/avcodec.h>
#include <libswresample/swresample.h>
}

#include <util/log.h>

#include <cassert>

struct FFMPEGAtrac9Info {
    uint32_t version;
    uint32_t config_data;
    uint32_t padding;
};

void convert_f32_to_s16(const float *f32, int16_t *s16, uint32_t dest_channels, uint32_t source_channels, uint32_t samples, uint32_t freq) {
    const int source_channel_type = source_channels == 2 ? AV_CH_LAYOUT_STEREO : AV_CH_LAYOUT_MONO;
    const int dest_channel_type = dest_channels == 2 ? AV_CH_LAYOUT_STEREO : AV_CH_LAYOUT_MONO;

    SwrContext *swr = swr_alloc_set_opts(nullptr,
        dest_channel_type, AV_SAMPLE_FMT_S16, freq,
        source_channel_type, AV_SAMPLE_FMT_FLTP, freq,
        0, nullptr);

    swr_init(swr);

    const int result = swr_convert(swr, (uint8_t **)&s16, samples, (const uint8_t**)(f32), samples);
    swr_free(&swr);
    assert(result > 0);
}

// maybe turn these back into public funcs thanks to DecoderQuery?
uint32_t Atrac9DecoderState::get_channel_count() {
    const std::uint8_t block_rate_index = ((config_data & (0b111 << 9)) >> 9);
    std::uint32_t total_channels = 0;

    switch (block_rate_index) {
        case 0:     // Mono
            total_channels = 1;
            break;

        case 1:     // Dual Mono (Mono, Mono)
            total_channels = 2;
            break;

        case 2:     // Stereo
            total_channels = 2;
            break;

        case 3:     // Stereo, Mono, LFE, Stereo
            total_channels = 2 + 1 + 1 + 2;
            break;

        case 4:     // Stereo, Mono, LFE, Stereo, Stereo
            total_channels = 2 + 1 + 1 + 2 + 2;
            break;

        case 5:     // Dual Stereo
            total_channels = 4;
            break;

        default:
            total_channels = 2;
            break;
    }

    return total_channels;
}

uint32_t Atrac9DecoderState::get_samples_per_superframe() {
    const std::uint8_t superframe_index = (config_data & (0b11 << 27)) >> 27;
    const std::uint8_t sample_rate_index = ((config_data & (0b1111 << 12)) >> 12);
    const std::uint32_t frame_per_superframe = 1 << superframe_index;

    static const std::int8_t sample_rate_index_to_frame_sample_power[] = {
        6, 6, 7, 7, 7, 8, 8, 8, 6, 6, 7, 7, 7, 8, 8, 8
    };

    const std::uint32_t samples_per_frame = 1 << sample_rate_index_to_frame_sample_power[sample_rate_index];
    const std::uint32_t samples_per_superframe = samples_per_frame * frame_per_superframe;

    return samples_per_superframe;
}

uint32_t Atrac9DecoderState::get_block_align() {
    // How many channel presents?
    return get_samples_per_superframe() * 4 * get_channel_count();
}

uint32_t Atrac9DecoderState::get_frames_in_superframe() {
    const std::uint8_t superframe_index = (config_data & (0b11 << 27)) >> 27;

    return 1 << superframe_index;
}

uint32_t Atrac9DecoderState::get_superframe_size() {
    const std::uint16_t frame_bytes = ((((config_data & 0xFF0000) >> 16) << 3) | ((config_data & (0b111 << 29)) >> 29)) + 1;
    return frame_bytes * get_frames_in_superframe();
}

uint32_t Atrac9DecoderState::get(DecoderQuery query) {
    switch (query) {
        case DecoderQuery::CHANNELS: return context->channels;
        case DecoderQuery::BIT_RATE: return context->bit_rate;
        case DecoderQuery::SAMPLE_RATE: return context->sample_rate;
        case DecoderQuery::AT9_BLOCK_ALIGN: return get_block_align();
        case DecoderQuery::AT9_SAMPLE_PER_SUPERFRAME: return get_samples_per_superframe();
        case DecoderQuery::AT9_FRAMES_IN_SUPERFRAME: return get_frames_in_superframe();
        case DecoderQuery::AT9_SUPERFRAME_SIZE: return get_superframe_size();
        default: return 0;
    }
}

bool Atrac9DecoderState::send(const uint8_t *data, uint32_t size) {
    AVPacket *packet = av_packet_alloc();

    std::vector<uint8_t> temporary(size + AV_INPUT_BUFFER_PADDING_SIZE);
    memcpy(temporary.data(), data, size);

    packet->size = size;
    packet->data = temporary.data();

    int err = avcodec_send_packet(context, packet);
    av_packet_free(&packet);
    if (err < 0) {
        LOG_WARN("Error sending Atrac9 packet: {}.", log_hex(static_cast<uint32_t>(err)));
        return false;
    }

    return true;
}

bool Atrac9DecoderState::receive(uint8_t *data, DecoderSize *size) {
    AVFrame *frame = av_frame_alloc();

    int err = avcodec_receive_frame(context, frame);
    if (err < 0) {
        LOG_WARN("Error receiving Atrac9 frame: {}.", log_hex(static_cast<uint32_t>(err)));
        av_frame_free(&frame);
        return false;
    }

    assert(frame->format == AV_SAMPLE_FMT_FLTP);
    assert(get(DecoderQuery::AT9_SAMPLE_PER_SUPERFRAME) == frame->nb_samples);

    if (data) {
        convert_f32_to_s16(
            reinterpret_cast<float *>(frame->extended_data),
            reinterpret_cast<int16_t *>(data), 2,
            frame->channels, frame->nb_samples, frame->sample_rate);
    }

    if (size) {
        size->samples = frame->nb_samples;
    }

    av_frame_free(&frame);
    return true;
}

Atrac9DecoderState::Atrac9DecoderState(uint32_t config_data) : config_data(config_data) {
    AVCodec *codec = avcodec_find_decoder(AV_CODEC_ID_ATRAC9);
    assert(codec);

    context = avcodec_alloc_context3(codec);
    assert(context);

    FFMPEGAtrac9Info info = { };
    info.version = 2;
    info.config_data = config_data;
    info.padding = 0;

    // Block align is size of a superframe
    context->block_align = get_block_align();
    context->extradata_size = sizeof(FFMPEGAtrac9Info);
    context->extradata = reinterpret_cast<uint8_t *>(&info);

    int err = avcodec_open2(context, codec, nullptr);
    assert(err == 0);
}
