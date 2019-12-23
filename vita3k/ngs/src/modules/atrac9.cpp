#include <ngs/modules/atrac9.h>
#include <util/log.h>
#include <util/bytes.h>

extern "C" {
#include <libavcodec/avcodec.h>
}

namespace emu::ngs::atrac9 {
    std::unique_ptr<emu::ngs::Module> VoiceDefinition::new_module() {
        return std::make_unique<Module>();
    }

    struct FFMPEG_ATRAC9Info {
        std::uint32_t version;
        std::uint32_t config_data;
        std::uint32_t padding[2];

        explicit FFMPEG_ATRAC9Info()
            : version(2)
            , config_data(0) {
        }
    };

    static_assert(sizeof(FFMPEG_ATRAC9Info) == 16);
    
    Module::Module() 
        : emu::ngs::Module(emu::ngs::BUSS_ATRAC9)
        , context(nullptr) {
        if (!init()) {
            LOG_WARN("ATRAC9 module is not supported!");
        }
    }

    bool Module::init() {
        codec = avcodec_find_decoder(AV_CODEC_ID_ATRAC9);
        
        if (!codec) {
            LOG_ERROR("Can't find ATRAC9 decoder on FFMPEG!");
            return false;
        }

        context = avcodec_alloc_context3(codec);

        if (!context) {
            LOG_ERROR("Can't alloc context for ATRAC9 module!");
            return false;
        }

        return true;
    }

    static std::uint32_t calculate_channel_count(const std::uint32_t config_data) {
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

    static std::uint32_t calculate_sample_per_superframe(const std::uint32_t config_data) {
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

    static std::uint32_t calculate_block_align(const std::uint32_t config_data) { 
        // How many channel presents?
        return calculate_sample_per_superframe(config_data) * 4 * calculate_channel_count(config_data);
    }

    static std::uint32_t calculate_superframe_size(const std::uint32_t config_data) {        
        const std::uint8_t superframe_index = (config_data & (0b11 << 27)) >> 27;
        const std::uint16_t frame_bytes = ((((config_data & 0xFF0000) >> 16) << 3) | ((config_data & (0b111 << 29)) >> 29)) + 1;
        const std::uint32_t frame_per_superframe = 1 << superframe_index;
        return frame_bytes * frame_per_superframe;
   }

    void get_buffer_parameter(const std::uint32_t start_sample, const std::uint32_t 
        num_samples, const std::uint32_t info, SkipBufferInfo &parameter) {
        const std::uint8_t sample_rate_index = ((info & (0b1111 << 12)) >> 12);
        const std::uint8_t block_rate_index = ((info & (0b111 << 9)) >> 9);
        const std::uint16_t frame_bytes = ((((info & 0xFF0000) >> 16) << 3) | ((info & (0b111 << 29)) >> 29)) + 1;
        const std::uint8_t superframe_index = (info & (0b11 << 27)) >> 27;

        // Calculate bytes per superframe.
        const std::uint32_t frame_per_superframe = 1 << superframe_index;
        const std::uint32_t bytes_per_superframe = frame_bytes * frame_per_superframe;

        // Calculate total superframe
        static const std::int8_t sample_rate_index_to_frame_sample_power[] = {
            6, 6, 7, 7, 7, 8, 8, 8, 6, 6, 7, 7, 7, 8, 8, 8
        };

        const std::uint32_t samples_per_frame = 1 << sample_rate_index_to_frame_sample_power[sample_rate_index];
        const std::uint32_t samples_per_superframe = samples_per_frame * frame_per_superframe;

        const std::uint32_t num_superframe = (num_samples + samples_per_superframe - 1) / samples_per_superframe;
        parameter.num_bytes = num_superframe * bytes_per_superframe;
        parameter.is_super_packet = (frame_per_superframe == 1) ? 0 : 1;

        const std::uint32_t start_superframe = (start_sample / superframe_index);
        parameter.start_byte_offset = start_superframe * bytes_per_superframe;
        parameter.start_skip = (start_sample - (start_superframe * samples_per_superframe));
        parameter.end_skip = (num_superframe * samples_per_superframe) - num_samples;
    }

    void Module::process(const MemState &mem, Voice *voice) {
        Parameters *params = voice->get_parameters<Parameters>(mem);
        State *state = voice->get_state<State>();

        assert(state);

        if (state->current_buffer == -1) {
            return;
        }

        const std::uint32_t block_align = calculate_block_align(params->config_data);
        const std::uint32_t superframe_size = calculate_superframe_size(params->config_data);
        const std::uint32_t sample_per_superframe = calculate_sample_per_superframe(params->config_data);

        if (!route(mem, voice, nullptr, params->channels, sample_per_superframe, static_cast<int>(params[state->current_buffer].playback_frequency),
            AudioDataType::F32)) {
            // Ran out of data, supply new
            // Decode new data and deliver them
            // Let's open our context
            FFMPEG_ATRAC9Info info;
            info.config_data = params->config_data;

            // Block align is size of a superframe
            context->block_align = block_align;
            context->extradata_size = 12;
            context->extradata = reinterpret_cast<std::uint8_t*>(&info);

            int res = avcodec_open2(context, codec, nullptr);

            AVPacket *packet = av_packet_alloc();

            // If out of data, read a superframe
            packet->size = superframe_size;
            packet->data = reinterpret_cast<std::uint8_t*>(params->buffer_params[state->current_buffer].
                buffer.get(mem)) + state->current_byte_position_in_buffer;

            res = avcodec_send_packet(context, packet);
            av_packet_free(&packet);

            if (res != 0) {
                LOG_ERROR("Unable to send ATRAC9 packet!");
                return;
            }

            // Try to receive the frame
            AVFrame *frame = av_frame_alloc();
            res = avcodec_receive_frame(context, frame);

            if (res != 0) {
                LOG_ERROR("Unable to receive ATRAC9 frame!");
                av_frame_free(&frame);
                return;
            }

            route(mem, voice, frame->extended_data, params->channels, frame->nb_samples, static_cast<int>(params[state->current_buffer].playback_frequency),
                AudioDataType::F32);

            state->samples_generated_since_key_on += sample_per_superframe;
            state->bytes_consumed_since_key_on += superframe_size;
            state->current_byte_position_in_buffer += superframe_size;
            state->total_bytes_consumed += superframe_size;

            if (state->bytes_consumed_since_key_on >= params->buffer_params[state->current_buffer].bytes_count) {
                state->current_buffer = params->buffer_params[state->current_buffer].next_buffer_index;
            }

            av_frame_free(&frame);
        }
    }
};