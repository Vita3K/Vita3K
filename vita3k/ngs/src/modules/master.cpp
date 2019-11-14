#include <ngs/modules/master.h>
#include <util/log.h>

namespace emu::ngs::master {
    std::unique_ptr<emu::ngs::Module> VoiceDefinition::new_module() {
        return std::make_unique<Module>();
    }
    
    Module::Module() 
        : emu::ngs::Module(emu::ngs::BUSS_MASTER) {
    }

    void Module::process(const MemState &mem, Voice *voice) {
        int a = 5;
    }
};