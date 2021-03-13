#include <ngs/modules/player.h>
#include <util/log.h>

#include <cassert>

namespace ngs::player {
Module::Module()
    : ngs::Module(ngs::BussType::BUSS_NORMAL_PLAYER) {}

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

    std::uint8_t *data_ptr = data.extra_storage.data();

    if (!decoder || (decoder->he_adpcm != static_cast<bool>(params->type))) {
        decoder = std::make_unique<PCMDecoderState>(data.parent->rack->system->sample_rate);
        decoder->he_adpcm = static_cast<bool>(params->type);
    }

    if (static_cast<std::int32_t>(state->decoded_gran_pending) < data.parent->rack->system->granularity) {
        if (!data.extra_storage.empty()) {
            data.extra_storage.erase(data.extra_storage.begin(), data.extra_storage.begin() + state->decoded_gran_passed * 8);
        } else {
            if (state->current_buffer == -1) {
                return;
            }
        }

        state->decoded_gran_passed = 0;

        while (static_cast<std::int32_t>(state->decoded_gran_pending) < data.parent->rack->system->granularity) {
            // Ran out of data, supply new
            // Decode new data and deliver them
            // Let's open our context
            if ((data.extra_storage.size() < data.parent->rack->system->granularity * 2 * sizeof(float)) && (state->current_buffer != -1)) {
                decoder->source_channels = params->channels;
                decoder->source_frequency = params->playback_frequency;

                auto *input = params->buffer_params[state->current_buffer].buffer.cast<uint8_t>().get(mem);

                DecoderSize samples_count;

                decoder->send(input, params->buffer_params[state->current_buffer].bytes_count);
                decoder->receive(nullptr, &samples_count);

                const std::size_t current_count = data.extra_storage.size();

                data.extra_storage.resize(current_count + samples_count.samples * sizeof(float));
                decoder->receive(current_count + data.extra_storage.data(), nullptr);
            }

            data_ptr = data.extra_storage.data();

            std::uint32_t bytes_left_in_buffer = data.extra_storage.size();
            std::uint32_t samples_to_take_per_channel = bytes_left_in_buffer / sizeof(float) / 2;

            state->bytes_consumed_since_key_on += params->buffer_params[state->current_buffer].bytes_count;
            state->current_byte_position_in_buffer += params->buffer_params[state->current_buffer].bytes_count;
            state->total_bytes_consumed += params->buffer_params[state->current_buffer].bytes_count;

            state->decoded_gran_pending = samples_to_take_per_channel;

            if (state->current_buffer == -1) {
                break;
            } else {
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
                                data.invoke_callback(kern, mem, thread_id, SCE_NGS_PLAYER_CALLBACK_REASON_DONE_ALL, 0, 0);
                                // TODO: Free all occupied input routes
                                //unroute_occupied(mem, voice);
                            } else {
                                data.invoke_callback(kern, mem, thread_id, SCE_NGS_PLAYER_CALLBACK_REASON_DONE_ONE_BUFFER, prev_index,
                                    params->buffer_params[state->current_buffer].buffer.address());
                            }

                            data.parent->voice_lock->lock();
                        }
                    } else {
                        data.parent->voice_lock->unlock();
                        data.invoke_callback(kern, mem, thread_id, SCE_NGS_PLAYER_CALLBACK_REASON_START_LOOP, state->current_loop_count,
                            params->buffer_params[state->current_buffer].buffer.address());
                        data.parent->voice_lock->lock();
                    }

                    state->current_byte_position_in_buffer = 0;
                }
            }
        }
    }

    std::uint32_t gran_to_be_passed = std::min<std::uint32_t>(state->decoded_gran_pending, data.parent->rack->system->granularity);
    data_ptr += 2 * sizeof(float) * state->decoded_gran_passed;

    data.parent->products[0] = data_ptr;

    state->decoded_gran_pending = (state->decoded_gran_pending < state->decoded_gran_passed) ? 0 : (state->decoded_gran_pending - gran_to_be_passed);
    state->decoded_gran_passed += gran_to_be_passed;

    state->samples_generated_since_key_on += gran_to_be_passed * 2;
}
} // namespace ngs::player
