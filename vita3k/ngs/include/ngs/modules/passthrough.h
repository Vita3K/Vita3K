#pragma once

#include <ngs/system.h>

namespace ngs::passthrough {
struct Module : public ngs::Module {
    Module();

    void process(KernelState &kern, const MemState &mem, const SceUID thread_id, ModuleData &data) override;
    std::size_t get_buffer_parameter_size() const override;
};
} // namespace ngs::passthrough
