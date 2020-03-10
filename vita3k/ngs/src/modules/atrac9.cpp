#include <ngs/modules/atrac9.h>
#include <util/log.h>
#include <util/bytes.h>

namespace ngs::atrac9 {
    std::unique_ptr<ngs::Module> VoiceDefinition::new_module() {
        return std::make_unique<Module>();
    }

    std::size_t VoiceDefinition::get_buffer_parameter_size() const {
        return sizeof(Parameters);
    }
    
    Module::Module() : ngs::Module(ngs::BussType::BUSS_ATRAC9) { }

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
        // Lock voice to avoid resource modificiation from other thread
        const std::lock_guard<std::mutex> guard(*voice->voice_lock);

        Parameters *params = voice->get_parameters<Parameters>(mem);
        State *state = voice->get_state<State>();

        if (state->current_buffer == -1) {
            return;
        }

        assert(state);

//        const std::uint32_t block_align = calculate_block_align(params->config_data);
//        const std::uint32_t superframe_size = calculate_superframe_size(params->config_data);
//        const std::uint32_t sample_per_superframe = calculate_sample_per_superframe(params->config_data);

        // making this maybe to early...
        decoder = std::make_unique<Atrac9DecoderState>(params->config_data);

        if (!route(mem, voice, nullptr, params->channels, decoder->get_samples_per_superframe(),
            static_cast<int>(params[state->current_buffer].playback_frequency), AudioDataType::F32)) {
            // Ran out of data, supply new
            // Decode new data and deliver them
            // Let's open our context
            auto *input = params->buffer_params[state->current_buffer].buffer.cast<uint8_t>().get(mem)
                + state->current_byte_position_in_buffer;
            std::vector<uint8_t> output(decoder->get_samples_per_superframe() * sizeof(int16_t));
            decoder->send(input, decoder->get_superframe_size());
            decoder->receive(output.data(), nullptr);

            route(mem, voice, output.data(), params->channels, decoder->get_samples_per_superframe(),
                static_cast<int>(params[state->current_buffer].playback_frequency), AudioDataType::F32);

            state->samples_generated_since_key_on += decoder->get_samples_per_superframe();
            state->bytes_consumed_since_key_on += decoder->get_superframe_size();
            state->current_byte_position_in_buffer += decoder->get_superframe_size();
            state->total_bytes_consumed += decoder->get_superframe_size();

            if (state->current_byte_position_in_buffer >= params->buffer_params[state->current_buffer].bytes_count) {
                if (params->buffer_params[state->current_buffer].loop_count != -1) {
                    state->current_loop_count++;

                    if (state->current_loop_count > params->buffer_params[state->current_buffer].loop_count) {
                        state->current_buffer = params->buffer_params[state->current_buffer].next_buffer_index;
                        state->current_loop_count = 0;
                        
                        if (state->current_buffer == -1) {
                            // Free all occupied input routes
                            unroute_occupied(mem, voice);
                        }
                    }
                }

                state->current_byte_position_in_buffer = 0;
            }
        }
    }
};