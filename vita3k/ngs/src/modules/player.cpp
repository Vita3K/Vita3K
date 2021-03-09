#include <ngs/modules/player.h>

#include <cassert>

namespace ngs::player {
void VoiceDefinition::new_modules(std::vector<std::unique_ptr<ngs::Module>> &mods) {
    mods.push_back(std::make_unique<Module>());
}

std::size_t VoiceDefinition::get_total_buffer_parameter_size() const {
    return sizeof(Parameters);
}

Module::Module()
    : ngs::Module(ngs::BussType::BUSS_NORMAL_PLAYER) {}

std::size_t Module::get_buffer_parameter_size() const {
    return sizeof(Parameters);
}

void Module::process(KernelState &kern, const MemState &mem, const SceUID thread_id, ModuleData &data) {
    Parameters *params = data.get_parameters<Parameters>(mem);

    // TODO: unimplemented, should play audio through sdl I think

    if (data.parent->inputs.inputs.empty()) {
        return;
    }

    assert(data.parent->inputs.inputs.size() == 1);
    uint8_t *output_data = data.parent->inputs.inputs[0].data();

    deliver_data(mem, data.parent, 0, output_data);
}
} // namespace ngs::player
