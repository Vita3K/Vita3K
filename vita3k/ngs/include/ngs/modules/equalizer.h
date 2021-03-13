#pragma once

#include <ngs/system.h>

namespace ngs::equalizer {
struct Module : public ngs::Module {
public:
    explicit Module();

    void process(KernelState &kern, const MemState &mem, const SceUID thread_id, ModuleData &data) override;
    std::uint32_t module_id() const override { return 0x5CEC; }
    std::size_t get_buffer_parameter_size() const override;
};
}; // namespace ngs::equalizer
