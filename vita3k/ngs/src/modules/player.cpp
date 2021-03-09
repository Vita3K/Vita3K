#include <ngs/modules/player.h>

#include <cassert>

namespace ngs::player {
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
    data.parent->products[0] = data.parent->inputs.inputs[0].data();
}
} // namespace ngs::player
