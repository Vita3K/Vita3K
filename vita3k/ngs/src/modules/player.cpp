#include <ngs/modules/player.h>

#include <cassert>

namespace ngs::player {
std::size_t VoiceDefinition::get_buffer_parameter_size() const {
    return sizeof(Parameters);
}

std::unique_ptr<ngs::Module> VoiceDefinition::new_module() {
    return std::make_unique<Module>();
}

Module::Module()
    : ngs::Module(ngs::BussType::BUSS_NORMAL_PLAYER) {}

void Module::process(const MemState &mem, Voice *voice) {
    Parameters *params = voice->get_parameters<Parameters>(mem);

    const std::lock_guard<std::mutex> guard(*voice->voice_lock);

    // TODO: unimplemented, should play audio through sdl I think

    if (voice->inputs.inputs.empty()) {
        return;
    }

    assert(voice->inputs.inputs.size() == 1);
    uint8_t *output_data = voice->inputs.inputs[0].data();

    deliver_data(mem, voice, 0, output_data);
}
} // namespace ngs::player
