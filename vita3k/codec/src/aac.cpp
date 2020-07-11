#include <codec/state.h>

#define DEBUG

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavformat/avio.h>
#include <libavutil/log.h>
#include <libavutil/opt.h>
#include <libswresample/swresample.h>
}

#include <util/log.h>

size_t AacDecoderState::read_packet_cache(uint8_t *data, size_t size) {
    size_t data_read = 0;

    while (data_read < size && packet_coarse_index < packet_cache.size()) {
        std::vector<uint8_t> &current_packet = packet_cache[packet_coarse_index];
        size_t data_available = std::min<size_t>(size - data_read, current_packet.size() - packet_fine_index);

        std::memcpy(&data[data_read], &current_packet[packet_fine_index], data_available);
        packet_fine_index += data_available;
        data_read += data_available;

        if (packet_fine_index >= current_packet.size()) {
            current_packet.clear();
            packet_coarse_index++;
            packet_fine_index = 0;
        }
    }

    if (data_read < size) {
        LOG_WARN("Out of packet data.");
    }

    return data_read;
}

uint32_t AacDecoderState::get(DecoderQuery query) {
    switch (query) {
    case DecoderQuery::CHANNELS: return context->channels;
    case DecoderQuery::BIT_RATE: return context->bit_rate;
    case DecoderQuery::SAMPLE_RATE: return context->sample_rate;
    default:
        return 0;
    }
}

bool AacDecoderState::send(const uint8_t *data, uint32_t size) {
    packet_cache.emplace_back(data, data + size); // add to packet cache

    // format context needs to be built when there is at least some data
    if (!format_context) {
        format_context = std::shared_ptr<AVFormatContext>(avformat_alloc_context(), &avformat_free_context);
        AVFormatContext *format_ptr = format_context.get();
        assert(format_ptr);

        format_ptr->pb = io_context.get();

        int err = avformat_open_input(&format_ptr, "", nullptr, nullptr);
        assert(err == 0);

        //        AVCodec *codec = nullptr;
        //        stream_id = av_find_best_stream(format_ptr, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
        //        assert(stream_id >= 0);
        //        assert(context->codec == codec);
    }

    AVPacket *packet = av_packet_alloc();
    int err = av_read_frame(format_context.get(), packet);
    if (err != 0) {
        LOG_INFO("Error reading frame: {}.", codec_error_name(err));
        return false;
    }

    err = avcodec_send_packet(context, packet);
    av_packet_free(&packet);
    if (err < 0) {
        LOG_WARN("Error sending Aac packet: {}.", codec_error_name(err));
        return false;
    }

    return true;
}

bool AacDecoderState::receive(uint8_t *data, DecoderSize *size) {
    AVFrame *frame = av_frame_alloc();

    int err = avcodec_receive_frame(context, frame);
    if (err < 0) {
        LOG_WARN("Error receiving Aac frame: {}.", codec_error_name(err));
        av_frame_free(&frame);
        return false;
    }

    assert(frame->format == AV_SAMPLE_FMT_FLTP);

    if (data) {
        convert_f32_to_s16(
            reinterpret_cast<float *>(frame->extended_data),
            reinterpret_cast<int16_t *>(data),
            frame->channels, frame->channels,
            frame->nb_samples, frame->sample_rate);
    }

    if (size) {
        size->samples = frame->nb_samples;
    }

    av_frame_free(&frame);
    return true;
}

AacDecoderState::AacDecoderState(uint32_t sample_rate, uint32_t channels) {
    constexpr size_t buffer_size = 8192;
    io_buffer = std::shared_ptr<uint8_t>(reinterpret_cast<uint8_t *>(av_malloc(buffer_size)), &av_free);
    io_context = std::shared_ptr<AVIOContext>(
        avio_alloc_context(
            io_buffer.get(), buffer_size, // buffer data
            false, // writeable
            this, // user data
            [](void *user, uint8_t *data, int size) -> int {
                auto *decoder = reinterpret_cast<AacDecoderState *>(user);

                return decoder->read_packet_cache(data, size);
            }, // read
            nullptr,
            nullptr),
        &avio_close);

    AVCodec *codec = avcodec_find_decoder(AV_CODEC_ID_AAC);
    assert(codec);

    context = avcodec_alloc_context3(codec);
    assert(context);

    context->channels = channels;
    context->sample_rate = sample_rate;

    int err = avcodec_open2(context, codec, nullptr);
    assert(err == 0);
}
