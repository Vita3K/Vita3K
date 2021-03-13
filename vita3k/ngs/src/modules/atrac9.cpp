#include <ngs/modules/atrac9.h>
#include <util/bytes.h>
#include <util/log.h>

namespace ngs::atrac9 {
Module::Module()
    : ngs::Module(ngs::BussType::BUSS_ATRAC9)
    , last_config(0) {}

void get_buffer_parameter(const std::uint32_t start_sample, const std::uint32_t num_samples, const std::uint32_t info, SkipBufferInfo &parameter) {
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

std::size_t Module::get_buffer_parameter_size() const {
    return sizeof(Parameters);
}

void Module::on_state_change(ModuleData &data, const VoiceState previous) {
    State *state = data.get_state<State>();
    if (data.parent->state == VOICE_STATE_AVAILABLE) {
        state->current_byte_position_in_buffer = 0;
        state->current_loop_count = 0;
        state->current_buffer = 0;
    }
}

void Module::process(KernelState &kern, const MemState &mem, const SceUID thread_id, ModuleData &data) {
    Parameters *params = data.get_parameters<Parameters>(mem);
    State *state = data.get_state<State>();

    assert(state);

    if (state->current_buffer == -1) {
        return;
    }

    // making this maybe to early...
    if (!decoder || (params->config_data != last_config)) {
        decoder = std::make_unique<Atrac9DecoderState>(params->config_data);
        last_config = params->config_data;
    }

    if (static_cast<std::int32_t>(state->decoded_samples_pending) < data.parent->rack->system->granularity) {
        // Ran out of data, supply new
        // Decode new data and deliver them
        // Let's open our context
        auto *input = params->buffer_params[state->current_buffer].buffer.cast<uint8_t>().get(mem)
            + state->current_byte_position_in_buffer;

        data.extra_storage.resize(decoder->get_samples_per_superframe() * sizeof(float) * 2);
        decoder->send(input, decoder->get_superframe_size());
        decoder->receive(data.extra_storage.data(), nullptr);

        state->samples_generated_since_key_on += decoder->get_samples_per_superframe();
        state->bytes_consumed_since_key_on += decoder->get_superframe_size();
        state->current_byte_position_in_buffer += decoder->get_superframe_size();
        state->total_bytes_consumed += decoder->get_superframe_size();

        state->decoded_samples_pending += decoder->get_samples_per_superframe();
        state->decoded_passed = 0;

        if (state->current_byte_position_in_buffer >= params->buffer_params[state->current_buffer].bytes_count) {
            const std::int32_t prev_index = state->current_buffer;

            if (params->buffer_params[state->current_buffer].loop_count != -1) {
                state->current_loop_count++;

                if (state->current_loop_count > params->buffer_params[state->current_buffer].loop_count) {
                    state->current_buffer = params->buffer_params[state->current_buffer].next_buffer_index;
                    state->current_loop_count = 0;
                    state->current_byte_position_in_buffer = 0;

                    data.parent->voice_lock->unlock();

                    if (state->current_buffer == -1) {
                        data.invoke_callback(kern, mem, thread_id, SCE_NGS_AT9_CALLBACK_REASON_DONE_ALL, 0, 0);
                        // TODO: Free all occupied input routes
                        //unroute_occupied(mem, voice);
                    } else {
                        data.invoke_callback(kern, mem, thread_id, SCE_NGS_AT9_CALLBACK_REASON_DONE_ONE_BUFFER, prev_index,
                            params->buffer_params[state->current_buffer].buffer.address());
                    }

                    data.parent->voice_lock->lock();
                }
            } else {
                data.parent->voice_lock->unlock();
                data.invoke_callback(kern, mem, thread_id, SCE_NGS_AT9_CALLBACK_REASON_START_LOOP, state->current_loop_count,
                    params->buffer_params[state->current_buffer].buffer.address());
                data.parent->voice_lock->lock();
            }

            state->current_byte_position_in_buffer = 0;
        }
    }

    std::uint8_t *data_ptr = data.extra_storage.data() + params->channels * sizeof(float) * state->decoded_passed;
    std::uint32_t samples_to_be_passed = data.parent->rack->system->granularity;

    data.parent->products[0] = data_ptr;

    state->decoded_samples_pending = (state->decoded_samples_pending < samples_to_be_passed) ? 0 : (state->decoded_samples_pending - samples_to_be_passed);
    state->decoded_passed += samples_to_be_passed;
}
}; // namespace ngs::atrac9
