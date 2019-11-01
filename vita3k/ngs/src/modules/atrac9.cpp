#include <ngs/modules/atrac9.h>

namespace emu::ngs::atrac9 {
    Module::Module() 
        : VoiceDefinition(emu::ngs::BUSS_ATRAC9) {
    }

    void Module::process(const MemState &mem, Voice *voice) {
        Parameters *params = voice->get_parameters<Parameters>(mem);
        int a = 5;
    }
};