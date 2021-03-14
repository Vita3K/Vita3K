#include <ngs/modules/null.h>

namespace ngs::null {
Module::Module()
    : ngs::Module(ngs::BussType::BUSS_MASTER) {}

std::size_t Module::get_buffer_parameter_size() const {
    return 0;
}

void Module::process(KernelState &kern, const MemState &mem, const SceUID thread_id, ModuleData &data) {
}
} // namespace ngs::null
