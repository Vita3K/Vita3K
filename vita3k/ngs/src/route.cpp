#include <ngs/system.h>

extern "C" {
#include <libswresample/swresample.h>
#include <libavutil/opt.h>
}

#include <util/log.h>

namespace emu::ngs {
    static AVSampleFormat audio_data_type_to_sample_format(AudioDataType data_type) {
        switch (data_type) {
        case AudioDataType::F32:
            return AV_SAMPLE_FMT_FLTP;

        case AudioDataType::S16:
            return AV_SAMPLE_FMT_S16;

        default:
            break;
        }

        return AV_SAMPLE_FMT_S16;
    } 

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
        const int output_channel_type = (output_channels == 1) ? AV_CH_LAYOUT_MONO : AV_CH_LAYOUT_STEREO;
        
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
                const int dest_channel_type = (patch->dest_channels == 1) ? AV_CH_LAYOUT_MONO : AV_CH_LAYOUT_STEREO;

                SwrContext *swr = swr_alloc();
                av_opt_set_int(swr, "in_channel_layout", output_channel_type, 0);
                av_opt_set_int(swr, "out_channel_layout", dest_channel_type,  0);
                av_opt_set_int(swr, "in_sample_rate", freq, 0);
                av_opt_set_int(swr, "out_sample_rate", freq, 0);
                av_opt_set_sample_fmt(swr, "in_sample_fmt",  audio_data_type_to_sample_format(output_type), 0);
                av_opt_set_sample_fmt(swr, "out_sample_fmt", audio_data_type_to_sample_format(patch->dest_data_type),  0);
                swr_init(swr);

                std::uint8_t *converted = nullptr;
                av_samples_alloc(&converted, nullptr, patch->dest_channels, sample_count, audio_data_type_to_sample_format(patch->dest_data_type), 0);
                const int result = swr_convert(swr, &converted, sample_count, const_cast<const std::uint8_t**>(output_data), sample_count);

                if (result != sample_count) {
                    LOG_ERROR("Unable to fully resample and route!");
                    return false;
                }

                patch->dest_sub_index = patch->dest->inputs.receive(patch->dest_index, patch->dest_sub_index,
                    &converted, output_channels, converted_channel_size);
                
                av_freep(&converted);
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