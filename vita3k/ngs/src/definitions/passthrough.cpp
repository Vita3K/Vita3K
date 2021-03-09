#include <ngs/definitions/passthrough.h>
#include <ngs/modules/passthrough.h>

namespace ngs::passthrough {
void VoiceDefinition::new_modules(std::vector<std::unique_ptr<ngs::Module>> &mods) {
    mods.push_back(std::make_unique<Module>());

    // Reserve some spaces
    for (int i = 0; i < 9; i++) {
        mods.push_back(nullptr);
    }
}

std::size_t VoiceDefinition::get_total_buffer_parameter_size() const {
    return default_parameter_size * 10;
}
} // namespace ngs::passthrough
