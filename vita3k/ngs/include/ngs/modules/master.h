#pragma once

#include <ngs/system.h>

namespace ngs::master {
struct Module : public ngs::Module {
public:
    explicit Module();
    bool process(KernelState &kern, const MemState &mem, const SceUID thread_id, ModuleData &data) override;
    std::size_t get_buffer_parameter_size() const override {
        return 0;
    }
};
} // namespace ngs::master
