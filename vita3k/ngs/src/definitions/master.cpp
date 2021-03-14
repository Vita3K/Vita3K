#include <ngs/definitions/master.h>
#include <ngs/modules/master.h>
#include <ngs/modules/null.h>

namespace ngs::master {
void VoiceDefinition::new_modules(std::vector<std::unique_ptr<ngs::Module>> &mods) {
    mods.push_back(std::make_unique<ngs::null::Module>());
    mods.push_back(std::make_unique<Module>());
}

std::size_t VoiceDefinition::get_total_buffer_parameter_size() const {
    return 0;
}
}; // namespace ngs::master
