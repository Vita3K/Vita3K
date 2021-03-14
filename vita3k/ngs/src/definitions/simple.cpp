#include <ngs/definitions/simple.h>
#include <ngs/modules/atrac9.h>
#include <ngs/modules/equalizer.h>
#include <ngs/modules/player.h>
#include <util/log.h>

namespace ngs::simple {
void PlayerVoiceDefinition::new_modules(std::vector<std::unique_ptr<ngs::Module>> &mods) {
    LOG_TRACE("Simple player voice definition stubbed");

    mods.push_back(std::make_unique<ngs::player::Module>());
    mods.push_back(std::make_unique<ngs::equalizer::Module>());
    mods.push_back(nullptr);
    mods.push_back(nullptr);
    mods.push_back(nullptr);
    mods.push_back(nullptr);
}

std::size_t PlayerVoiceDefinition::get_total_buffer_parameter_size() const {
    return sizeof(ngs::player::Parameters) + default_normal_parameter_size * 5;
}

void Atrac9VoiceDefinition::new_modules(std::vector<std::unique_ptr<ngs::Module>> &mods) {
    LOG_TRACE("AT9 player voice definition stubbed");

    mods.push_back(std::make_unique<ngs::atrac9::Module>());
    mods.push_back(std::make_unique<ngs::equalizer::Module>());
    mods.push_back(nullptr);
    mods.push_back(nullptr);
    mods.push_back(nullptr);
    mods.push_back(nullptr);
}

std::size_t Atrac9VoiceDefinition::get_total_buffer_parameter_size() const {
    return sizeof(ngs::atrac9::Parameters) + default_normal_parameter_size * 5;
}

} // namespace ngs::simple
