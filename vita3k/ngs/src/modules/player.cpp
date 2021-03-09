#include <ngs/modules/player.h>
#include <util/log.h>

#include <cassert>

namespace ngs::player {
Module::Module()
    : ngs::Module(ngs::BussType::BUSS_NORMAL_PLAYER)
    , decoded_samples_pending(0)
    , decoded_passed(0) {}

std::size_t Module::get_buffer_parameter_size() const {
    return sizeof(Parameters);
}

void Module::process(KernelState &kern, const MemState &mem, const SceUID thread_id, ModuleData &data) {
    Parameters *params = data.get_parameters<Parameters>(mem);
    State *state = data.get_state<State>();

    if (data.parent->inputs.inputs.empty()) {
        return;
    }

    if (state->current_buffer == -1) {
        return;
    }

    std::uint8_t *data_ptr = nullptr;

    if (static_cast<std::int32_t>(decoded_samples_pending) < data.parent->rack->system->granularity) {
        if (!decoded_pending.empty())
            decoded_pending.erase(decoded_pending.begin(), decoded_pending.begin() + decoded_passed * 4);

        decoded_passed = 0;
        while (static_cast<std::int32_t>(decoded_samples_pending) < data.parent->rack->system->granularity) {
            // Ran out of data, supply new
            // Decode new data and deliver them
            // Let's open our context
            auto *input = params->buffer_params[state->current_buffer].buffer.cast<uint8_t>().get(mem)
                + state->current_byte_position_in_buffer;

            if (params->type == ParameterAudioTypeADPCM) {
                LOG_ERROR("ADPCM audio, not supported yet!!");
                return;
            } else {
                // TODO: Accelerate
                std::size_t current_pos = decoded_pending.size();

                std::uint32_t bytes_left_in_buffer = params->buffer_params[state->current_buffer].bytes_count - state->current_byte_position_in_buffer;
                std::uint32_t samples_to_take = bytes_left_in_buffer / sizeof(std::uint16_t) / params->channels;
                samples_to_take = std::min<std::uint32_t>(samples_to_take, data.parent->rack->system->granularity);

                bytes_left_in_buffer = samples_to_take * params->channels * sizeof(std::uint16_t);
                decoded_pending.resize(current_pos + bytes_left_in_buffer);

                if (params->channels == 2) {
                    std::memcpy(&decoded_pending[current_pos], input, bytes_left_in_buffer);
                } else {
                    for (std::size_t i = 0; i < bytes_left_in_buffer; i++) {
                        decoded_pending[current_pos + 2 * i] = input[i];
                        decoded_pending[current_pos + 2 * i + 1] = input[i + 1];
                    }
                }

                data_ptr = decoded_pending.data();

                state->samples_generated_since_key_on += samples_to_take;
                state->bytes_consumed_since_key_on += params->channels * samples_to_take * sizeof(std::uint16_t);
                state->current_byte_position_in_buffer += params->channels * samples_to_take * sizeof(std::uint16_t);
                state->total_bytes_consumed += params->channels * samples_to_take * sizeof(std::uint16_t);

                decoded_samples_pending += samples_to_take;
            }

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

    std::uint32_t samples_to_be_passed = std::min<std::uint32_t>(decoded_samples_pending, data.parent->rack->system->granularity);
    data_ptr += 2 * sizeof(std::int16_t) * decoded_passed;

    data.parent->products[0] = data_ptr;

    decoded_samples_pending = (decoded_samples_pending < samples_to_be_passed) ? 0 : (decoded_samples_pending - samples_to_be_passed);
    decoded_passed += samples_to_be_passed;
}
} // namespace ngs::player
