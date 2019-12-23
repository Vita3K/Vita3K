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
            
            const std::uint32_t converted_channel_size = sample_count * audio_data_type_to_byte_count(patch->dest_data_type);

            patch->dest->receive(patch->dest_index, &converted, output_channels, converted_channel_size);
            av_freep(&converted);
        }

        return true;
    }
}