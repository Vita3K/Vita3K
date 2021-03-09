#include <ngs/modules/equalizer.h>
#include <util/log.h>

namespace ngs::equalizer {
Module::Module()
    : ngs::Module(ngs::BussType::BUSS_EQUALIZATION) {}

std::size_t Module::get_buffer_parameter_size() const {
    return default_parameter_size;
}

void Module::get_expectation(AudioDataType *expect_audio_type, std::int16_t *expect_channel_count) {
    *expect_audio_type = AudioDataType::S16;
    *expect_channel_count = 2;
}

void Module::process(KernelState &kern, const MemState &mem, const SceUID thread_id, ModuleData &data) {
    if (data.parent->inputs.inputs.size() < 1) {
        return;
    }

    assert(data.parent->inputs.inputs.size() == 1);

    // It should do some modifications to create 4 outputs, but I'm not sure what yet kkk
    data.parent->products[1] = data.parent->products[0];
    data.parent->products[2] = data.parent->products[0];
    data.parent->products[3] = data.parent->products[0];
}
} // namespace ngs::equalizer
