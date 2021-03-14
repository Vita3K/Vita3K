#include <ngs/modules/passthrough.h>
#include <util/log.h>

namespace ngs::passthrough {
Module::Module()
    : ngs::Module(ngs::BussType::BUSS_REVERB) {}

std::size_t Module::get_buffer_parameter_size() const {
    return default_passthrough_parameter_size;
}

void Module::process(KernelState &kern, const MemState &mem, const SceUID thread_id, ModuleData &data) {
    if (data.parent->inputs.inputs.size() < 1) {
        return;
    }

    assert(data.parent->inputs.inputs.size() == 1);
    data.parent->products[0].data = data.parent->inputs.inputs[0].data();
}
} // namespace ngs::passthrough
