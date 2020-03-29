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

    bool deliver_data(const MemState &mem, Voice *source, const std::uint8_t output_port,
        const std::uint8_t *output_data) {
        if (!output_data) {
            return false;
        }

        for (std::size_t i = 0; i < source->patches[output_port].size(); i++) {
            Patch *patch = source->patches[output_port][i].get(mem);

            if (!patch || patch->output_sub_index == -1) {
                continue;
            }
            
            if (output_data != nullptr) {
                patch->dest->inputs.receive(patch, &output_data);
            }
        }

        return true;
    }
}