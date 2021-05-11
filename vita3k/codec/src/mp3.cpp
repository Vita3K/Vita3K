#include <codec/state.h>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
#include <libswresample/swresample.h>
}

#include <util/log.h>

#include <cassert>

// Code from https://hydrogenaud.io/index.php?topic=85125.0, author: SirNickity

// MPEG versions - use [version]
const uint8_t mpeg_versions[4] = { 25, 0, 2, 1 };

// Layers - use [layer]
const uint8_t mpeg_layers[4] = { 0, 3, 2, 1 };

// Bitrates - use [version][layer][bitrate]
const uint16_t mpeg_bitrates[4][4][16] = {
    {
        // Version 2.5
        { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }, // Reserved
        { 0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160, 0 }, // Layer 3
        { 0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160, 0 }, // Layer 2
        { 0, 32, 48, 56, 64, 80, 96, 112, 128, 144, 160, 176, 192, 224, 256, 0 } // Layer 1
    },
    {
        // Reserved
        { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }, // Invalid
        { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }, // Invalid
        { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }, // Invalid
        { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 } // Invalid
    },
    {
        // Version 2
        { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }, // Reserved
        { 0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160, 0 }, // Layer 3
        { 0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160, 0 }, // Layer 2
        { 0, 32, 48, 56, 64, 80, 96, 112, 128, 144, 160, 176, 192, 224, 256, 0 } // Layer 1
    },
    {
        // Version 1
        { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }, // Reserved
        { 0, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 0 }, // Layer 3
        { 0, 32, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 384, 0 }, // Layer 2
        { 0, 32, 64, 96, 128, 160, 192, 224, 256, 288, 320, 352, 384, 416, 448, 0 }, // Layer 1
    }
};

// Sample rates - use [version][srate]
const uint16_t mpeg_srates[4][4] = {
    { 11025, 12000, 8000, 0 }, // MPEG 2.5
    { 0, 0, 0, 0 }, // Reserved
    { 22050, 24000, 16000, 0 }, // MPEG 2
    { 44100, 48000, 32000, 0 } // MPEG 1
};

// Samples per frame - use [version][layer]
const uint16_t mpeg_frame_samples[4][4] = {
    //    Rsvd     3     2     1  < Layer  v Version
    { 0, 576, 1152, 384 }, //       2.5
    { 0, 0, 0, 0 }, //       Reserved
    { 0, 576, 1152, 384 }, //       2
    { 0, 1152, 1152, 384 } //       1
};

// Slot size (MPEG unit of measurement) - use [layer]
const uint8_t mpeg_slot_size[4] = { 0, 1, 1, 4 }; // Rsvd, 3, 2, 1

uint32_t Mp3DecoderState::get_es_size(const uint8_t *data) {
    // Quick validity check
    if (((data[0] & 0xFF) != 0xFF)
        || ((data[1] & 0xE0) != 0xE0) // 3 sync bits
        || ((data[1] & 0x18) == 0x08) // Version rsvd
        || ((data[1] & 0x06) == 0x00) // Layer rsvd
        || ((data[2] & 0xF0) == 0xF0) // Bitrate rsvd
    ) {
        return 0;
    }

    // Data to be extracted from the header
    uint8_t ver = (data[1] & 0x18) >> 3; // Version index
    uint8_t lyr = (data[1] & 0x06) >> 1; // Layer index
    uint8_t pad = (data[2] & 0x02) >> 1; // Padding? 0/1
    uint8_t brx = (data[2] & 0xf0) >> 4; // Bitrate index
    uint8_t srx = (data[2] & 0x0c) >> 2; // SampRate index

    // Lookup real values of these fields
    uint32_t bitrate = mpeg_bitrates[ver][lyr][brx] * 1000;
    uint32_t samprate = mpeg_srates[ver][srx];
    uint16_t samples = mpeg_frame_samples[ver][lyr];
    uint8_t slot_size = mpeg_slot_size[lyr];

    // In-between calculations
    float bps = static_cast<float>(samples) / 8.0f;
    float fsize = ((bps * static_cast<float>(bitrate)) / static_cast<float>(samprate)) + ((pad) ? slot_size : 0);

    // Frame sizes are truncated integers
    return static_cast<uint16_t>(fsize);
}

uint32_t Mp3DecoderState::get(DecoderQuery query) {
    switch (query) {
    case DecoderQuery::CHANNELS: return context->channels;
    default: return 0;
    }
}

bool Mp3DecoderState::send(const uint8_t *data, uint32_t size) {
    AVPacket *packet = av_packet_alloc();

    std::vector<uint8_t> temporary(size + AV_INPUT_BUFFER_PADDING_SIZE);
    memcpy(temporary.data(), data, size);

    packet->size = size;
    packet->data = temporary.data();

    int err = avcodec_send_packet(context, packet);
    av_packet_free(&packet);
    if (err < 0) {
        LOG_WARN("Error sending Mp3 packet: {}.", log_hex(static_cast<uint32_t>(err)));
        return false;
    }

    return true;
}

bool Mp3DecoderState::receive(uint8_t *data, DecoderSize *size) {
    AVFrame *frame = av_frame_alloc();

    int err = avcodec_receive_frame(context, frame);
    if (err < 0) {
        LOG_WARN("Error receiving Mp3 frame: {}.", log_hex(static_cast<uint32_t>(err)));
        av_frame_free(&frame);
        return false;
    }

    if (data) {
        int data_size = av_get_bytes_per_sample(context->sample_fmt);

        for (int i = 0; i < frame->nb_samples; i++) {
            for (int ch = 0; ch < context->channels; ch++) {
                for (int j = 0; j < data_size; j++) {
                    data[((data_size + context->channels) * i) + (ch * context->channels) + j] = frame->data[ch][data_size * i + j];
                }
            }
        }
    }

    if (size) {
        size->samples = frame->nb_samples;
    }

    av_frame_free(&frame);
    return true;
}

Mp3DecoderState::Mp3DecoderState(uint32_t channels) {
    AVCodec *codec = avcodec_find_decoder(AV_CODEC_ID_MP3);
    assert(codec);

    context = avcodec_alloc_context3(codec);
    assert(context);

    context->channels = channels;

    int err = avcodec_open2(context, codec, nullptr);
    assert(err == 0);
}

Mp3DecoderState::~Mp3DecoderState() {
    avcodec_close(context);
    av_free(context);

    context = nullptr;
}
