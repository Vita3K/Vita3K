#pragma once

#include <ngs/system.h>

namespace emu::ngs::master {
    struct Module: public emu::ngs::Module {
    public:
        explicit Module();
        void process(const MemState &mem, Voice *voice) override;
    };

    struct VoiceDefinition: public emu::ngs::VoiceDefinition {
        std::size_t get_buffer_parameter_size() const override {
            return 0;
        }

        std::unique_ptr<emu::ngs::Module> new_module() override;
    };
};