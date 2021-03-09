#include <ngs/modules/passthrough.h>

#include <util/log.h>

namespace ngs::passthrough {
// random number of bytes to make sure nothing bad happens
constexpr size_t default_parameter_size = 256;

void VoiceDefinition::new_modules(std::vector<std::unique_ptr<ngs::Module>> &mods) {
    mods.push_back(std::make_unique<Module>());
}

std::size_t VoiceDefinition::get_total_buffer_parameter_size() const {
    return default_parameter_size;
}

Module::Module()
    : ngs::Module(ngs::BussType::BUSS_REVERB) {}

std::size_t Module::get_buffer_parameter_size() const {
    return default_parameter_size;
}

void Module::process(KernelState &kern, const MemState &mem, const SceUID thread_id, ModuleData &data) {
    if (data.parent->inputs.inputs.size() < 1) {
        return;
    }

    assert(data.parent->inputs.inputs.size() == 1);
    uint8_t *output_data = data.parent->inputs.inputs[0].data();

    deliver_data(mem, data.parent, 0, output_data);
}

void Module::get_expectation(AudioDataType *expect_audio_type, std::int16_t *expect_channel_count) {}
} // namespace ngs::passthrough