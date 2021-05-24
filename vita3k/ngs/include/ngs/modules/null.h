#pragma once

#include <ngs/system.h>

namespace ngs::null {
struct Module : public ngs::Module {
    Module();

    bool process(KernelState &kern, const MemState &mem, const SceUID thread_id, ModuleData &data) override;
    std::size_t get_buffer_parameter_size() const override;
};
} // namespace ngs::null
