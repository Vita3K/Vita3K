#pragma once

#include <ngs/modules/atrac9.h>
#include <ngs/modules/player.h>
#include <ngs/system.h>

namespace ngs::simple {
struct PlayerVoiceDefinition : public ngs::VoiceDefinition {
    void new_modules(std::vector<std::unique_ptr<ngs::Module>> &mods) override;
    std::size_t get_total_buffer_parameter_size() const override;
};

struct Atrac9VoiceDefinition : public ngs::VoiceDefinition {
    void new_modules(std::vector<std::unique_ptr<ngs::Module>> &mods) override;
    std::size_t get_total_buffer_parameter_size() const override;
};
}; // namespace ngs::simple