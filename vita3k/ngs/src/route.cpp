#include <ngs/system.h>

#include <util/log.h>
#include <codec/state.h>

namespace ngs {
    static int audio_data_type_to_byte_count(AudioDataType data_type) {
        switch (data_type) {
        case AudioDataType::F32:
            return 4;
        
        case AudioDataType::S16:
            return 2;

        default:
            break;
        }

        return 2;
    }

    bool route(const MemState &mem, Voice *source, std::uint8_t ** const output_data, const std::uint16_t output_channels, const std::uint32_t sample_count, const int freq, AudioDataType output_type) {
        for (std::size_t i = 0; i < source->outputs.size(); i++) {
            Patch *patch = source->outputs[i].get(mem);

            if (!patch || patch->output_sub_index == -1) {
                continue;
            }
            
            const std::uint32_t byte_per_dest_sample = audio_data_type_to_byte_count(patch->dest_data_type);
        
            VoiceInputManager::PCMBuf *input_buf = patch->dest->inputs.get_input_buffer_queue(patch->dest_index,
                patch->dest_sub_index);

            if (output_data == nullptr) {
                if (!input_buf || input_buf->size() <= patch->dest_channels * byte_per_dest_sample * patch->dest->rack->system->granularity) {
                    // Need more data.
                    return false;
                }
            }

            const std::uint32_t converted_channel_size = sample_count * byte_per_dest_sample;

            if (input_buf && input_buf->size() != 0) {
                // Delete last used samples
                input_buf->erase(input_buf->begin(), input_buf->begin() + std::min<std::size_t>(input_buf->size(),
                    patch->dest_channels * byte_per_dest_sample * patch->dest->rack->system->granularity));
            }

            if (output_data != nullptr) {
                // Supply newly converted data
                std::vector<int16_t> s16_data(sample_count);
                int16_t *data_ptr = s16_data.data();

                // TODO: error reporting
                convert_f32_to_s16(reinterpret_cast<float *>(output_data), data_ptr, output_channels, sample_count, freq);

                patch->dest_sub_index = patch->dest->inputs.receive(patch->dest_index, patch->dest_sub_index,
                    reinterpret_cast<uint8_t **>(&data_ptr), output_channels, converted_channel_size);
            }
        }

        return true;
    }

    bool unroute_occupied(const MemState &mem, Voice *source) {
        for (std::size_t i = 0; i < source->outputs.size(); i++) {
            Patch *patch = source->outputs[i].get(mem);

            if (!patch || patch->output_sub_index == -1) {
                continue;
            }
            
            patch->dest->inputs.free_input(patch->dest_index, patch->dest_sub_index);
            patch->dest_sub_index = -1;
        }

        return true;
    }
}