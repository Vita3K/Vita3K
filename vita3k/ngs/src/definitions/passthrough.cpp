#include <ngs/definitions/passthrough.h>
#include <ngs/modules/passthrough.h>

namespace ngs::passthrough {
void VoiceDefinition::new_modules(std::vector<std::unique_ptr<ngs::Module>> &mods) {
    mods.push_back(std::make_unique<Module>());
    mods.push_back(std::make_unique<Module>());
    mods.push_back(std::make_unique<Module>());
    mods.push_back(std::make_unique<Module>());
}

std::size_t VoiceDefinition::get_total_buffer_parameter_size() const {
    return default_passthrough_parameter_size * 4;
}
} // namespace ngs::passthrough
